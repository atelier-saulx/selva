/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "selva.h"
#include "cstrings.h"
#include "svector.h"
#include "arg_parser.h"
#include "hierarchy.h"
#include "modify.h"
#include "rpn.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "subscriptions.h"
#include "inherit_fields.h"

struct InheritFieldValue_Args {
    size_t first_node; /*!< We ignore the type of the first node. */
    size_t nr_types;
    const Selva_NodeType *types;
    RedisModuleString *lang;
    const char *field_name_str;
    size_t field_name_len;
    struct SelvaObjectAny *res;
};

struct InheritSendFields_Args {
    size_t nr_fields;
    RedisModuleString *lang;
    RedisModuleString **field_names;
    ssize_t nr_results; /*!< Number of results sent. */
};

struct InheritCommand_Args {
    size_t first_node; /*!< We ignore the type of the first node. */
    size_t nr_fields;
    size_t nr_types;
    const Selva_NodeType *types;
    RedisModuleString *lang;
    RedisModuleString **field_names;
    ssize_t nr_results; /*!< Number of results sent. */
};

static int Inherit_FieldValue_NodeCb(
        RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct InheritFieldValue_Args *restrict args = (struct InheritFieldValue_Args *)arg;
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    int err;

    /*
     * Check that the node is of an accepted type.
     */
    if (likely(!args->first_node)) {
        Selva_NodeId nodeId;
        int match = 0;

        SelvaHierarchy_GetNodeId(nodeId, node);

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

    err = Inherit_GetField(hierarchy, args->lang, node, obj, args->field_name_str, args->field_name_len, args->res);
    if (err == 0) {
        return 1; /* found */
    } else if (err != SELVA_ENOENT) {
        Selva_NodeId nodeId;

        SelvaHierarchy_GetNodeId(nodeId, node);

        /*
         * SELVA_ENOENT is expected as not all nodes have all fields set;
         * Any other error is unexpected.
         */
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to get a field value. nodeId: %.*s fieldName: \"%.*s\" error: %s\n",
                  (int)SELVA_NODE_ID_SIZE, nodeId,
                  (int)args->field_name_len, args->field_name_str,
                  getSelvaErrorStr(err));
    }

    return 0;
}

int Inherit_FieldValue(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
        const Selva_NodeId node_id,
        const Selva_NodeType *types,
        size_t nr_types,
        const char *field_name_str,
        size_t field_name_len,
        struct SelvaObjectAny *res) {
    struct InheritFieldValue_Args args = {
        .lang = lang,
        .first_node = 1,
        .nr_types = nr_types,
        .types = types,
        .field_name_str = field_name_str,
        .field_name_len = field_name_len,
        .res = res,
    };
    const struct SelvaHierarchyCallback cb = {
        .node_cb = Inherit_FieldValue_NodeCb,
        .node_arg = &args,
    };

    return SelvaHierarchy_Traverse(ctx, hierarchy, node_id, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS, &cb);
}

static int Inherit_SendFields_NodeCb(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct InheritSendFields_Args *restrict args = (struct InheritSendFields_Args *)arg;
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    int err;

    for (size_t i = 0; i < args->nr_fields; i++) {
        RedisModuleString *field_name = args->field_names[i];

        /* Field already found. */
        if (!field_name) {
            continue;
        }

        /*
         * Get and send the field value to the client.
         * The response should always start like this: [node_id, field_name, ...]
         * but we don't send the header yet.
         */
        TO_STR(field_name);
        err = Inherit_SendFieldFind(ctx, hierarchy, args->lang,
                                node, obj,
                                field_name, /* Initially full_field is the same as field_name. */
                                field_name_str, field_name_len);
        if (err == 0) { /* found */
            args->field_names[i] = NULL; /* No need to look for this one anymore. */
            args->nr_results++;

            /* Stop traversing if all fields were found. */
            if (args->nr_results == (ssize_t)args->nr_fields) {
                return 1;
            }
        } else if (err != SELVA_ENOENT) {
            Selva_NodeId nodeId;

            SelvaHierarchy_GetNodeId(nodeId, node);

            /*
             * SELVA_ENOENT is expected as not all nodes have all fields set;
             * Any other error is unexpected.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to get a field value. nodeId: %.*s fieldName: \"%s\" error: %s\n",
                      (int)SELVA_NODE_ID_SIZE, nodeId,
                      RedisModule_StringPtrLen(field_name, NULL),
                      getSelvaErrorStr(err));
        }
    }

    return 0;
}

int Inherit_SendFields(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
        const Selva_NodeId node_id,
        RedisModuleString **field_names,
        size_t nr_field_names) {
    struct InheritSendFields_Args args = {
        .lang = lang,
        .field_names = RedisModule_PoolAlloc(ctx, nr_field_names * sizeof(RedisModuleString *)),
        .nr_fields = nr_field_names,
        .nr_results = 0,
    };
    const struct SelvaHierarchyCallback cb = {
        .node_cb = Inherit_SendFields_NodeCb,
        .node_arg = &args,
    };
    int err;

    memcpy(args.field_names, field_names, nr_field_names * sizeof(RedisModuleString *));
    err = SelvaHierarchy_Traverse(ctx, hierarchy, node_id, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS, &cb);
    if (err) {
        /* TODO Better error handling? */
        SELVA_LOG(SELVA_LOGL_ERR, "Inherit failed: %s", getSelvaErrorStr(err));
    }

    return args.nr_results;
}

static int InheritCommand_NodeCb(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct InheritCommand_Args *restrict args = (struct InheritCommand_Args *)arg;
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    int err;

    /*
     * Check that the node is of an accepted type.
     */
    if (likely(!args->first_node)) {
        Selva_NodeId nodeId;
        int match = 0;

        SelvaHierarchy_GetNodeId(nodeId, node);

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
         * The response should always start like this: [node_id, field_name, ...]
         * but we don't send the header yet.
         */
        TO_STR(field_name);
        err = Inherit_SendField(ctx, hierarchy, args->lang,
                                node, obj,
                                field_name, /* Initially full_field is the same as field_name. */
                                field_name_str, field_name_len);
        if (err == 0) { /* found */
            args->field_names[i] = NULL; /* No need to look for this one anymore. */
            args->nr_results++;

            /* Stop traversing if all fields were found. */
            if (args->nr_results == (ssize_t)args->nr_fields) {
                return 1;
            }
        } else if (err != SELVA_ENOENT) {
            Selva_NodeId nodeId;

            SelvaHierarchy_GetNodeId(nodeId, node);

            /*
             * SELVA_ENOENT is expected as not all nodes have all fields set;
             * Any other error is unexpected.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to get a field value. nodeId: %.*s fieldName: \"%s\" error: %s",
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    RedisModule_StringPtrLen(field_name, NULL),
                    getSelvaErrorStr(err));
        }
    }

    return 0;
}

size_t inheritHierarchyFields(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        Selva_NodeId node_id,
        size_t nr_types,
        const Selva_NodeType *types,
        size_t nr_field_names,
        RedisModuleString **field_names) {
    size_t nr_presolved = 0;

    for (size_t i = 0; i < nr_field_names; i++) {
        RedisModuleString *field_name = field_names[i];
        TO_STR(field_name);
        int err;

        err = 1; /* This value will help us know if something matched. */

#define IS_FIELD(name) \
        (!strcmp(field_name_str, name))

        /*
         * If the field_name is a hierarchy field the reply format is:
         * [node_id, field_name, [nodeId1, nodeId2,.. nodeIdn]]
         */
        if (IS_FIELD(SELVA_ANCESTORS_FIELD)) {
            RedisModule_ReplyWithArray(ctx, 3);
            RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
            RedisModule_ReplyWithString(ctx, field_name);
            err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS);
        } else if (IS_FIELD(SELVA_CHILDREN_FIELD)) {
            RedisModule_ReplyWithArray(ctx, 3);
            RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
            RedisModule_ReplyWithString(ctx, field_name);
            err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_CHILDREN);
        } else if (IS_FIELD(SELVA_DESCENDANTS_FIELD)) {
            RedisModule_ReplyWithArray(ctx, 3);
            RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
            RedisModule_ReplyWithString(ctx, field_name);
            err = HierarchyReply_WithTraversal(ctx, hierarchy, node_id, nr_types, types, SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS);
        } else if (IS_FIELD(SELVA_PARENTS_FIELD)) {
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
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to get a field value. nodeId: %.*s fieldName: \"%s\" error: %s",
                      (int)SELVA_NODE_ID_SIZE, node_id,
                      field_name_str,
                      getSelvaErrorStr(err));
        }
    }

    return nr_presolved;
#undef IS_FIELD
}

/**
 * Find node in set.
 * SELVA.inherit REDIS_KEY NODE_ID [TYPE1[TYPE2[...]]] [FIELD_NAME1[ FIELD_NAME2[ ...]]]
 */
int SelvaInheritCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    SelvaHierarchy *hierarchy;
    Selva_NodeId node_id;
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
    hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Get the node_id.
     */
    err = Selva_RMString2NodeId(node_id, argv[ARGV_NODE_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "node_id");
    }

    /*
     * Get types.
     */
    size_t nr_types;
    const Selva_NodeType *types = (char const (*)[SELVA_NODE_TYPE_SIZE])RedisModule_StringPtrLen(argv[ARGV_TYPES], &nr_types);

    if (nr_types % SELVA_NODE_TYPE_SIZE != 0) {
        return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "types");
    }
    nr_types /= SELVA_NODE_TYPE_SIZE;

    /*
     * Get field names.
     */
    const size_t nr_field_names = argc - ARGV_FIELD_NAMES;
    RedisModuleString **field_names = RedisModule_PoolAlloc(ctx, nr_field_names * sizeof(RedisModuleString *));

    memcpy(field_names, argv + ARGV_FIELD_NAMES, nr_field_names * sizeof(RedisModuleString *));

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run inherit for ancestors, descendants, parents, and children.
     * These fields will always exist in every node so these can be always
     * resolved at the top level.
     */
    size_t nr_presolved = inheritHierarchyFields(ctx, hierarchy, node_id, nr_types, types, nr_field_names, field_names);

    /*
     * Execute a traversal to inherit the requested field values.
     */
    struct InheritCommand_Args args = {
        .lang = lang,
        .first_node = 1,
        .nr_types = nr_types,
        .types = types,
        .field_names = field_names,
        .nr_fields = nr_field_names,
        .nr_results = nr_presolved,
    };
    const struct SelvaHierarchyCallback cb = {
        .node_cb = InheritCommand_NodeCb,
        .node_arg = &args,
    };

    err = SelvaHierarchy_Traverse(ctx, hierarchy, node_id, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS, &cb);
    RedisModule_ReplySetArrayLength(ctx, args.nr_results);

    if (err) {
        /*
         * We can't reply with an error anymore, so we just log it.
         */
        SELVA_LOG(SELVA_LOGL_ERR, "Inherit failed: %s", getSelvaErrorStr(err));
    }

    return REDISMODULE_OK;
}

static int Inherit_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.inherit", SelvaInheritCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Inherit_OnLoad);
