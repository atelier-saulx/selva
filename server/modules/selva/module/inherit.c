#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "selva.h"
#include "selva_onload.h"
#include "arg_parser.h"
#include "errors.h"
#include "hierarchy.h"
#include "rpn.h"
#include "subscriptions.h"
#include "svector.h"

struct InheritCommand_Args {
    RedisModuleCtx *ctx;

    size_t nr_types;
    const Selva_NodeType *types;
    RedisModuleString **field_names;
    size_t nr_fields;
    ssize_t nr_results; /*!< Number of results sent. */
};

static int get_field_value(RedisModuleCtx *ctx, const Selva_NodeId nodeId, const RedisModuleString *field, RedisModuleString **out) {
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

    /* TODO Check for magic values */

    if (err || !value) {
        return SELVA_ENOENT;
    }

    *out = value;
    return 0;
}

static int InheritCommand_NodeCb(Selva_NodeId nodeId, void *arg, struct SelvaModify_HierarchyMetadata *metadata __unused) {
    struct InheritCommand_Args *restrict args = (struct InheritCommand_Args *)arg;
    int err;
    int match = 0;

    /*
     * Check that the node is of an accepted type.
     */
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

    for (size_t i = 0; i < args->nr_fields; i++) {
        RedisModuleString *field_name = args->field_names[i];
        RedisModuleString *value;

        /* Field already found. */
        if (!field_name) {
            continue;
        }

        /*
         * Get the field value.
         */
        err = get_field_value(args->ctx, nodeId, field_name, &value);
        if (err == 0 && value) { /* found */
            args->field_names[i] = NULL; /* No need to look for this one anymore. */

            (void)RedisModule_ReplyWithArray(args->ctx, 2);
            (void)RedisModule_ReplyWithString(args->ctx, field_name);
            (void)RedisModule_ReplyWithString(args->ctx, value);
            args->nr_results++;

            /* Stop traversing if all fields were found. */
            if (args->nr_results == args->nr_fields) {
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

    const size_t ARGV_REDIS_KEY         = 1;
    const size_t ARGV_NODE_ID           = 2;
    const size_t ARGV_TYPES             = 3;
    const size_t ARGV_FIELD_NAMES       = 4;

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

    /*
     * Execute a traversal to inherit the requested field values.
     */
    struct InheritCommand_Args args = {
        .ctx = ctx,
        .nr_types = nr_types,
        .types = types,
        .field_names = field_names,
        .nr_fields = nr_field_names,
        .nr_results = 0,
    };
    const struct SelvaModify_HierarchyCallback cb = {
        .node_cb = InheritCommand_NodeCb,
        .node_arg = &args,
    };

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    err = SelvaModify_TraverseHierarchy(hierarchy, node_id, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS, &cb);
    RedisModule_ReplySetArrayLength(ctx, args.nr_results);

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
