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

struct AggregateCommand_Args {
    RedisModuleCtx *ctx;
    RedisModuleString *lang;

    RedisModuleString **field_names;
    size_t nr_fields;
    ssize_t nr_results; /*!< Number of results sent. */
};

static int send_edge_field_value(RedisModuleCtx *ctx, const Selva_NodeId node_id, RedisModuleString *field, struct EdgeField *edge_field) {
    RedisModule_ReplyWithArray(ctx, 3);
    RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
    RedisModule_ReplyWithString(ctx, field);
    replyWithEdgeField(ctx, edge_field);

    return 0;
}

static int send_object_field_value(RedisModuleCtx *ctx, RedisModuleString *lang, const Selva_NodeId node_id, struct SelvaObject *obj, RedisModuleString *field) {
    int err = SELVA_ENOENT;

    if (!SelvaObject_Exists(obj, field)) {
        /*
         * Start a new array reply:
         * [node_id, field_name, field_value]
         */
        RedisModule_ReplyWithArray(ctx, 3);
        RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
        RedisModule_ReplyWithString(ctx, field);

        err = SelvaObject_ReplyWithObject(ctx, lang, obj, field);
        if (err) {
            TO_STR(field);

            (void)replyWithSelvaErrorf(ctx, err, "failed to inherit field: \"%.*s\"", (int)field_len, field_str);
        }
    }

    return err;
}

static int send_field_value(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        struct SelvaModify_HierarchyNode *node,
        const Selva_NodeId node_id,
        struct SelvaObject *obj,
        RedisModuleString *field) {
    TO_STR(field);
    struct EdgeField *edge_field;

    edge_field = Edge_GetField(node, field_str, field_len);
    return edge_field
        ? send_edge_field_value(ctx, node_id, field, edge_field)
        : send_object_field_value(ctx, lang, node_id, obj, field);
}

static int AggregateCommand_NodeCb(struct SelvaModify_HierarchyNode *node, void *arg) {
    Selva_NodeId nodeId;
    RedisModuleKey *key = NULL;
    struct SelvaObject *obj;
    struct AggregateCommand_Args *restrict args = (struct AggregateCommand_Args *)arg;
    int err;

    SelvaModify_HierarchyGetNodeId(nodeId, node);
    err = open_node_key(args->ctx, nodeId, &key, &obj);
    if (err) {
        fprintf(stderr, "%s:%d: Failed to open a node object. nodeId: %.*s error: %s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, nodeId,
                getSelvaErrorStr(err));
        /* Ignore errors. */
        return 0;
    }

    // TODO

    RedisModule_CloseKey(key);
    return 0;
}

size_t aggregateHierarchyFields(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        size_t nr_field_names,
        RedisModuleString **field_names) {
    size_t nr_presolved = 0;

    for (size_t i = 0; i < nr_field_names; i++) {
        RedisModuleString *field_name = field_names[i];
        TO_STR(field_name);
        int err;

        err = 1; /* This value will help us know if something matched. */

        /*
         * If the field_name is a hierarchy field the reply format is:
         * [node_id, field_name, [nodeId1, nodeId2,.. nodeIdn]]
         */
        // if (!strcmp(field_name_str, "ancestors")) {
        //     RedisModule_ReplyWithArray(ctx, 3);
        //     RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
        //     RedisModule_ReplyWithString(ctx, field_name);
        //     err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS);
        // } else if (!strcmp(field_name_str, "children")) {
        //     RedisModule_ReplyWithArray(ctx, 3);
        //     RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
        //     RedisModule_ReplyWithString(ctx, field_name);
        //     err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_CHILDREN);
        // } else if (!strcmp(field_name_str, "descendants")) {
        //     RedisModule_ReplyWithArray(ctx, 3);
        //     RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
        //     RedisModule_ReplyWithString(ctx, field_name);
        //     err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS);
        // } else if (!strcmp(field_name_str, "parents")) {
        //     RedisModule_ReplyWithArray(ctx, 3);
        //     RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
        //     RedisModule_ReplyWithString(ctx, field_name);
        //     err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_PARENTS);
        // }
        // TODO

        if (err <= 0) { /* Something was traversed. */
            field_names[i] = NULL; /* This field is now resolved. */
            nr_presolved++;
        }

        if (err < 0) {
            fprintf(stderr, "%s:%d: Failed to get a field value. nodeId: %.*s fieldName: \"%s\" error: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node_id, field_name_str, getSelvaErrorStr(err));
        }
    }

    return nr_presolved;
}

/**
 * Find node in set.
 * SELVA.inherit REDIS_KEY NODE_ID [TYPE1[TYPE2[...]]] [FIELD_NAME1[ FIELD_NAME2[ ...]]]
 */
int SelvaAggregateCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_LANG          = 1;
    const int ARGV_REDIS_KEY     = 2;
    const int ARGV_NODE_ID       = 3;
    const int ARGV_TYPES         = 4;
    const int ARGV_FIELD_NAMES   = 5;

    if (argc < ARGV_FIELD_NAMES + 1) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *lang = argv[ARGV_LANG];

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
     * Run inherit for ancestors, descendants, parents, and children.
     * These fields will always exist in every node so these are always resolved a top level.
     */
    size_t nr_presolved = aggregateHierarchyFields(ctx, hierarchy, node_id, nr_field_names, field_names);

    /*
     * Execute a traversal to inherit the requested field values.
     */
    struct AggregateCommand_Args args = {
        .ctx = ctx,
        .lang = lang,
        .field_names = field_names,
        .nr_fields = nr_field_names,
        .nr_results = nr_presolved,
    };
    const struct SelvaModify_HierarchyCallback cb = {
        .node_cb = AggregateCommand_NodeCb,
        .node_arg = &args,
    };

    // TODO: this needs to look more like a find
    err = SelvaModify_TraverseHierarchy(hierarchy, node_id, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS, &cb);
    RedisModule_ReplySetArrayLength(ctx, args.nr_results);

    if (err) {
        /*
         * We can't reply with an error anymore, so we just log it.
         */
        fprintf(stderr, "%s:%d: Inherit failed: %s\n",
                __FILE__, __LINE__,
                getSelvaErrorStr(err));
    }

    return REDISMODULE_OK;
}

static int Aggregate_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.aggregate", SelvaAggregateCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Aggregate_OnLoad);
