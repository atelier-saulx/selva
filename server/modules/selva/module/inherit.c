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
#include "selva_node.h"
#include "selva_object.h"
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

static int send_object_field_value(RedisModuleCtx *ctx, struct SelvaObject *obj, RedisModuleString *node_id, RedisModuleString *field) {
    int err = SELVA_ENOENT;

    if (!SelvaObject_Exists(obj, field)) {
        /*
         * Start a new array reply:
         * [node_id, field_name, field_value]
         */
        RedisModule_ReplyWithArray(ctx, 3);

        RedisModule_ReplyWithString(ctx, node_id);
        RedisModule_ReplyWithString(ctx, field);

        err = SelvaObject_ReplyWithObject(ctx, obj, field);
        if (err) {
            TO_STR(field)

            (void)replyWithSelvaErrorf(ctx, err, "failed to inherit field: \"%.*s\"", (int)field_len, field_str);
        }
    }

    return err;
}

static int send_field_value(
        RedisModuleCtx *ctx,
        const Selva_NodeId nodeId,
        RedisModuleString *field) {
    int err;

    RedisModuleString *id;
    id = RedisModule_CreateString(ctx, nodeId, Selva_NodeIdLen(nodeId));
    if (!id) {
        return SELVA_ENOMEM;
    }

    RedisModuleKey *key;
    key = RedisModule_OpenKey(ctx, id, REDISMODULE_READ);
    if (!key) {
        return SELVA_ENOENT;
    }

    struct SelvaObject *obj;
    err = SelvaObject_Key2Obj(RedisModule_OpenKey(ctx, id, REDISMODULE_READ), &obj);
    if (err) {
        return err;
    }

    /*
     * The response should always start like this: [node_id, field_name, ...]
     * but we don't send the header yet.
     */

    err = send_object_field_value(ctx, obj, id, field);

    RedisModule_CloseKey(key);

    return err;
}

static int InheritCommand_NodeCb(Selva_NodeId nodeId, void *arg, struct SelvaModify_HierarchyMetadata *metadata __unused) {
    struct InheritCommand_Args *restrict args = (struct InheritCommand_Args *)arg;
    int err;

    /*
     * Check that the node is of an accepted type.
     */
    if (likely(!args->first_node)) {
        int match = 0;

        for (size_t i = 0; i < args->nr_types; i++) {
            match |= memcmp(args->types[i], nodeId, SELVA_NODE_TYPE_SIZE) == 0;
        }
        if (!match && args->nr_types > 0) {
            /*
             * This node type is not accepted and we don't need to check whether
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

size_t inheritHierarchyFields(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        size_t nr_types,
        const Selva_NodeType *types,
        size_t nr_field_names,
        RedisModuleString **field_names) {
    size_t nr_presolved = 0;
    int err;

    for (size_t i = 0; i < nr_field_names; i++) {
        RedisModuleString *field_name = field_names[i];
        TO_STR(field_name)

        err = 1; /* This value will help us know if something matched. */

        /*
         * If the field_name is a hierarchy field the reply format is:
         * [node_id, field_name, [nodeId1, nodeId2,.. nodeIdn]]
         */
        if (!strcmp(field_name_str, "ancestors")) {
            RedisModule_ReplyWithArray(ctx, 3);
            RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
            RedisModule_ReplyWithString(ctx, field_name);
            err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS);
        } else if (!strcmp(field_name_str, "children")) {
            RedisModule_ReplyWithArray(ctx, 3);
            RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
            RedisModule_ReplyWithString(ctx, field_name);
            err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_CHILDREN);
        } else if (!strcmp(field_name_str, "descendants")) {
            RedisModule_ReplyWithArray(ctx, 3);
            RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
            RedisModule_ReplyWithString(ctx, field_name);
            err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS);
        } else if (!strcmp(field_name_str, "parents")) {
            RedisModule_ReplyWithArray(ctx, 3);
            RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
            RedisModule_ReplyWithString(ctx, field_name);
            err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_PARENTS);
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

    return nr_presolved;
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
    nr_types /= SELVA_NODE_TYPE_SIZE;

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
    size_t nr_presolved = inheritHierarchyFields(ctx, hierarchy, node_id, nr_types, types, nr_field_names, field_names);

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
        /*
         * We can't reply with an error anymore, so we just log it.
         */
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
