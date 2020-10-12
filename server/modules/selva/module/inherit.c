#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "selva.h"
#include "redismodule.h"
#include "arg_parser.h"
#include "errors.h"
#include "hierarchy.h"
#include "modify.h"
#include "rpn.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "subscriptions.h"
#include "svector.h"

struct InheritCommand_Args {
    RedisModuleCtx *ctx;

    size_t first_node; /*!< We ignore the type of the first node. */
    size_t nr_types;
    const Selva_NodeType *types;
    RedisModuleString **field_names;
    size_t nr_fields;
    ssize_t nr_results; /*!< Number of results sent. */
};

struct send_hierarchy_field_data {
    RedisModuleCtx *ctx;

    size_t skip;
    size_t len;
};

static int send_selva_set(RedisModuleCtx *ctx, const Selva_NodeId nodeId, RedisModuleString *field) {
    RedisModuleKey *key;
    size_t len = 0;

    key = SelvaSet_Open(ctx, nodeId, Selva_NodeIdLen(nodeId),
                        RedisModule_StringPtrLen(field, NULL));
    if (!key) {
        return SELVA_MODIFY_HIERARCHY_EINVAL;
    }

    /*
     * Start a new array reply:
     * [field_name, [set_value1, set_value2,.. set_valuen]]
     */
    RedisModule_ReplyWithArray(ctx, 2);
    RedisModule_ReplyWithString(ctx, field);
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    RedisModule_ZsetFirstInScoreRange(key, 0, 1, 0, 0);
    while (!RedisModule_ZsetRangeEndReached(key)) {
        double score;
        RedisModuleString *ele;

        ele = RedisModule_ZsetRangeCurrentElement(key, &score);

        RedisModule_ReplyWithString(ctx, ele);

        RedisModule_FreeString(ctx, ele);
        RedisModule_ZsetRangeNext(key);
        len++;
    }
    RedisModule_ZsetRangeStop(key);

    RedisModule_ReplySetArrayLength(ctx, len);

    return 0;
}

static int send_hash_field_value(RedisModuleCtx *ctx, const Selva_NodeId nodeId, RedisModuleString *field) {
    int err;
    RedisModuleString *id;
    RedisModuleKey *key;
    RedisModuleString *value = NULL;

    id = RedisModule_CreateString(ctx, nodeId, Selva_NodeIdLen(nodeId));
    if (!id) {
        return SELVA_ENOMEM;
    }

    key = RedisModule_OpenKey(ctx, id, REDISMODULE_READ);
    if (!key) {
        return SELVA_ENOENT;
    }

    err = RedisModule_HashGet(key, REDISMODULE_HASH_NONE, field, &value, NULL);
    RedisModule_CloseKey(key);

    if (err || !value) {
        return SELVA_ENOENT;
    }

    /*
     * Start a new array reply:
     * [ field_name, field_value ]
     */
    RedisModule_ReplyWithArray(ctx, 2);
    RedisModule_ReplyWithString(ctx, field);

    TO_STR(value);
    if (!strcmp(value_str, SELVA_SET_KEYWORD)) {
        /* Set */
        return send_selva_set(ctx, nodeId, field);
    } else {
        /* Regular value */
        (void)RedisModule_ReplyWithString(ctx, value);
    }

    return 0;
}

/*
 * Used for ancestors, children, descendants, parents
 */
static int send_hierarchy_field_NodeCb(Selva_NodeId nodeId, void *arg, struct SelvaModify_HierarchyMetadata *metadata __unused) {
    struct send_hierarchy_field_data *args = (struct send_hierarchy_field_data *)arg;

    /*
     * Some traversal modes needs to skip the first entry.
     */
    if (unlikely(args->skip)) {
        args->skip = 0;
        return 0;
    }

    RedisModule_ReplyWithStringBuffer(args->ctx, nodeId, Selva_NodeIdLen(nodeId));
    args->len++;

    return 0;
}

static int send_hierarchy_field(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId nodeId,
        RedisModuleString *field,
        enum SelvaModify_HierarchyTraversal dir) {
    const size_t skip =
        dir == SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS ||
        dir == SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS;
    struct send_hierarchy_field_data args = {
        .ctx = ctx,
        .skip = skip,
        .len = 0,
    };
    const struct SelvaModify_HierarchyCallback cb = {
        .node_cb = send_hierarchy_field_NodeCb,
        .node_arg = &args,
    };
    int err;

    /*
     * Start a new array reply:
     * [field_name, [nodeId1, nodeId2,.. nodeIdn]]
     */
    RedisModule_ReplyWithArray(ctx, 2);
    RedisModule_ReplyWithString(ctx, field);
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    err = SelvaModify_TraverseHierarchy(hierarchy, nodeId, dir, &cb);
    RedisModule_ReplySetArrayLength(ctx, args.len);

    return err;
}

static int send_field_value(
        RedisModuleCtx *ctx,
        const Selva_NodeId nodeId,
        RedisModuleString *field) {
    TO_STR(field);

    if (!strcmp(field_str, "aliases")) {
        return send_selva_set(ctx, nodeId, field);
    } else {
        return send_hash_field_value(ctx, nodeId, field);
    }
}

static int InheritCommand_NodeCb(Selva_NodeId nodeId, void *arg, struct SelvaModify_HierarchyMetadata *metadata __unused) {
    struct InheritCommand_Args *restrict args = (struct InheritCommand_Args *)arg;
    int err;
    int match = 0;

    /*
     * Check that the node is of an accepted type.
     */
    if (likely(!args->first_node)) {
        for (size_t i = 0; i < args->nr_types; i++) {
            match |= memcmp(args->types[i], nodeId, SELVA_NODE_TYPE_SIZE) == 0;
        }
        if (!match) {
            /*
             * This node type is not accepted and we don't need to check whether has
             * the field set.
             */
            return 0;
        }
    } else {
        args->first_node = 0;
    }

    for (size_t i = 0; i < args->nr_fields; i++) {
        RedisModuleString *field_name = args->field_names[i];

        /* Field already found. */
        if (!field_name) {
            continue;
        }

        /*
         * Get and send the field value to the client.
         */
        err = send_field_value(args->ctx, nodeId, field_name);
        if (err == 0) { /* found */
            args->field_names[i] = NULL; /* No need to look for this one anymore. */
            args->nr_results++;

            /* Stop traversing if all fields were found. */
            if (args->nr_results == (ssize_t)args->nr_fields) {
                return 1;
            }
        } else if (err != SELVA_ENOENT) {
            /*
             * SELVA_ENOENT is expected as not all nodes have all fields set;
             * Any other error is unexpected.
             */
            fprintf(stderr, "%s: Failed to get a field value. nodeId: %.*s fieldName: \"%s\" error: %s\n",
                    __FILE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    RedisModule_StringPtrLen(field_name, NULL),
                    getSelvaErrorStr(err));
        }
    }

    return 0;
}

/**
 * Find node in set.
 * SELVA.inherit REDIS_KEY NODE_ID [TYPE1[TYPE2[...]]] [FIELD_NAME1[ FIELD_NAME2[ ...]]]
 */
int SelvaInheritCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const size_t ARGV_REDIS_KEY     = 1;
    const size_t ARGV_NODE_ID       = 2;
    const size_t ARGV_TYPES         = 3;
    const size_t ARGV_FIELD_NAMES   = 4;

    if (argc < (int)(ARGV_FIELD_NAMES + 1)) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Get the node_id.
     */
    Selva_NodeId node_id;
    SelvaArgParser_NodeId(node_id, argv[ARGV_NODE_ID]);

    /*
     * Get types.
     */
    size_t nr_types;
    const Selva_NodeType *types = (char const (*)[2])RedisModule_StringPtrLen(argv[ARGV_TYPES], &nr_types);
    nr_types /= 2;

    /*
     * Get field names.
     */
    const size_t nr_field_names = argc - ARGV_FIELD_NAMES;
    RedisModuleString **field_names = RedisModule_PoolAlloc(ctx, nr_field_names * sizeof(RedisModuleString *));
    if (!field_names) {
        return replyWithSelvaError(ctx, SELVA_ENOMEM);
    }
    memcpy(field_names, argv + ARGV_FIELD_NAMES, nr_field_names * sizeof(RedisModuleString *));

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run inherit for ancestors, descendants, parents, and children
     * We have two reasons to do this here:
     * - These fields will always exist in every node so these are always resolved a top level
     * - Hierarchy traversal is not reentrant and calling it inside another traversal will stop the outter iteration
     */
    size_t nr_presolved = 0;
    for (size_t i = 0; i < nr_field_names; i++) {
        RedisModuleString *field_name = field_names[i];
        TO_STR(field_name);
        int err = 1; /* This will help us to know if something matched. */

        if (!strcmp(field_name_str, "ancestors")) {
            err = send_hierarchy_field(ctx, hierarchy, node_id, field_name, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS);
        } else if (!strcmp(field_name_str, "children")) {
            err = send_hierarchy_field(ctx, hierarchy, node_id, field_name, SELVA_HIERARCHY_TRAVERSAL_CHILDREN);
        } else if (!strcmp(field_name_str, "descendants")) {
            err = send_hierarchy_field(ctx, hierarchy, node_id, field_name, SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS);
        } else if (!strcmp(field_name_str, "parents")) {
            err = send_hierarchy_field(ctx, hierarchy, node_id, field_name, SELVA_HIERARCHY_TRAVERSAL_PARENTS);
        }

        if (err <= 0) { /* Something was traversed. */
            field_names[i] = NULL; /* This field is now resolved. */
            nr_presolved++;
        }

        if (err < 0) {
            fprintf(stderr, "%s: Failed to get a field value. nodeId: %.*s fieldName: \"%s\" error: %s\n",
                    __FILE__, (int)SELVA_NODE_ID_SIZE, node_id, field_name_str, getSelvaErrorStr(err));
        }
    }

    /*
     * Execute a traversal to inherit the requested field values.
     */
    struct InheritCommand_Args args = {
        .ctx = ctx,
        .first_node = 1,
        .nr_types = nr_types,
        .types = types,
        .field_names = field_names,
        .nr_fields = nr_field_names,
        .nr_results = nr_presolved,
    };
    const struct SelvaModify_HierarchyCallback cb = {
        .node_cb = InheritCommand_NodeCb,
        .node_arg = &args,
    };

    err = SelvaModify_TraverseHierarchy(hierarchy, node_id, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS, &cb);
    RedisModule_ReplySetArrayLength(ctx, args.nr_results);

    if (err) {
        /* TODO What to do with this error? */
        fprintf(stderr, "%s: %s\n", __FILE__, getSelvaErrorStr(err));
    }

    return REDISMODULE_OK;
}

static int Inherit_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.inherit", SelvaInheritCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Inherit_OnLoad);
