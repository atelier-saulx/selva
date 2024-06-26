#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "selva.h"
#include "redismodule.h"
#include "cstrings.h"
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
    SelvaModify_Hierarchy *hierarchy;
    RedisModuleString *lang;

    size_t first_node; /*!< We ignore the type of the first node. */
    size_t nr_types;
    const Selva_NodeType *types;
    RedisModuleString **field_names;
    size_t nr_fields;
    ssize_t nr_results; /*!< Number of results sent. */
};

static int send_field_value(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        RedisModuleString *lang,
        const struct SelvaModify_HierarchyNode *node,
        const Selva_NodeId node_id,
        struct SelvaObject *obj,
        RedisModuleString *full_field,
        const char *field_str,
        size_t field_len);

static int open_node_key(RedisModuleCtx *ctx, const Selva_NodeId nodeId, RedisModuleKey **key_out, struct SelvaObject **obj_out) {
    RedisModuleString *id;
    RedisModuleKey *key;
    int err;

    id = RedisModule_CreateString(ctx, nodeId, Selva_NodeIdLen(nodeId));
    if (!id) {
        return SELVA_ENOMEM;
    }

    key = RedisModule_OpenKey(ctx, id, REDISMODULE_READ);
    if (!key) {
        return SELVA_ENOENT;
    }

    *key_out = key;

    err = SelvaObject_Key2Obj(RedisModule_OpenKey(ctx, id, REDISMODULE_READ), obj_out);
    if (err) {
        return err;
    }

    return 0;
}

static int send_edge_field_value(RedisModuleCtx *ctx, const Selva_NodeId node_id, RedisModuleString *full_field, struct EdgeField *edge_field) {
    /*
     * Start a new array reply:
     * [node_id, field_name, field_value]
     */
    RedisModule_ReplyWithArray(ctx, 3);
    RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
    RedisModule_ReplyWithString(ctx, full_field);
    replyWithEdgeField(ctx, edge_field);

    return 0;
}

static int deref_single_ref(
        RedisModuleCtx *ctx,
        const struct EdgeField *edge_field,
        Selva_NodeId node_id_out,
        RedisModuleKey **key_out,
        struct SelvaObject **obj_out) {
    const struct EdgeFieldConstraint *constraint = edge_field->constraint;
    const struct SelvaModify_HierarchyNode *node;

    if (constraint) {
        if (!(constraint->flags & EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF)) {
            return SELVA_EINVAL; /* We can only deref fields from a single ref. */
        }
    }

    node = SVector_GetIndex(&edge_field->arcs, 0);
    if (!node) {
        return SELVA_ENOENT;
    }

    SelvaHierarchy_GetNodeId(node_id_out, node);
    return open_node_key(ctx, node_id_out, key_out, obj_out);
}

static int send_edge_field_deref_value(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        RedisModuleString *lang,
        RedisModuleString *full_field,
        struct EdgeField *edge_field,
        const char *field_str,
        size_t field_len) {
    RedisModuleKey *key = NULL;
    struct SelvaObject *obj;
    Selva_NodeId nodeId;
    int err;

    err = deref_single_ref(ctx, edge_field, nodeId, &key, &obj);
    if (err) {
        return err;
    }

    if (field_len == 1 && field_str[0] == '*') {
        /*
         * It's a wildcard and we should send the whole node object excluding
         * reference fields.
         */
        TO_STR(full_field);

        /*
         * Start a new array reply:
         * [node_id, field_name, field_value]
         */
        RedisModule_ReplyWithArray(ctx, 3);
        RedisModule_ReplyWithStringBuffer(ctx, nodeId, Selva_NodeIdLen(nodeId)); /* The actual node_id. */
        RedisModule_ReplyWithStringBuffer(ctx, full_field_str, full_field_len - 2); /* -2 to remove the `.*` suffix */
        SelvaObject_ReplyWithObject(ctx, lang, obj, NULL);
    } else {
        const struct SelvaModify_HierarchyNode *node;

        node = SelvaHierarchy_FindNode(hierarchy, nodeId);
        if (!node) {
            return SELVA_ENOENT; /* RFE Should we return SELVA_HIERARCHY_ENOENT? */
        }

        return send_field_value(ctx, hierarchy, lang, node, nodeId, obj, full_field, field_str, field_len);
    }

    RedisModule_CloseKey(key);
    return 0;
}

static int send_object_field_value(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        const Selva_NodeId node_id,
        struct SelvaObject *obj,
        RedisModuleString *full_field,
        const char *field_str,
        size_t field_len) {
    int err = SELVA_ENOENT;

    if (!SelvaObject_ExistsStr(obj, field_str, field_len)) {
        /*
         * Start a new array reply:
         * [node_id, field_name, field_value]
         */
        RedisModule_ReplyWithArray(ctx, 3);
        RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
        RedisModule_ReplyWithString(ctx, full_field);

        err = SelvaObject_ReplyWithObjectStr(ctx, lang, obj, field_str, field_len);
        if (err) {
            (void)replyWithSelvaErrorf(ctx, err, "failed to inherit field: \"%.*s\"", (int)field_len, field_str);
        }
    }

    return err;
}

static int send_field_value(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        RedisModuleString *lang,
        const struct SelvaModify_HierarchyNode *node,
        const Selva_NodeId node_id,
        struct SelvaObject *obj,
        RedisModuleString *full_field,
        const char *field_str,
        size_t field_len) {
    struct EdgeField *edge_field;
    /*
     * If field is an edge field then the client wants to get the value of it,
     * usually an array of node ids.
     */
    edge_field = Edge_GetField(node, field_str, field_len);
    if (edge_field) {
        return send_edge_field_value(ctx, node_id, full_field, edge_field);
    } else {
        /*
         * If field was not an edge field perhaps a substring of field is an edge field.
         */
        ssize_t n = field_len;

        while ((n = strrnchr(field_str, n, '.')) > 0) {
            edge_field = Edge_GetField(node, field_str, n);
            if (edge_field) {
                const char *rest_str = field_str + n + 1;
                const size_t rest_len = field_len - n - 1;

                return send_edge_field_deref_value(ctx, hierarchy, lang, full_field, edge_field, rest_str, rest_len);
            }
        }
    }

    /* Finally try from a node object field. */
    return send_object_field_value(ctx, lang, node_id, obj, full_field, field_str, field_len);
}

static int InheritCommand_NodeCb(struct SelvaModify_HierarchyNode *node, void *arg) {
    Selva_NodeId nodeId;
    RedisModuleKey *key = NULL;
    struct SelvaObject *obj;
    struct InheritCommand_Args *restrict args = (struct InheritCommand_Args *)arg;
    int err;

    SelvaHierarchy_GetNodeId(nodeId, node);
    err = open_node_key(args->ctx, nodeId, &key, &obj);
    if (err) {
        fprintf(stderr, "%s:%d: Failed to open a node object. nodeId: %.*s error: %s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, nodeId,
                getSelvaErrorStr(err));
        /* Ignore errors. */
        return 0;
    }

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
            RedisModule_CloseKey(key);
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
        err = send_field_value(args->ctx, args->hierarchy, args->lang,
                               node, nodeId, obj,
                               field_name, /* Initially full_field is the same as field_name. */
                               field_name_str, field_name_len);
        if (err == 0) { /* found */
            args->field_names[i] = NULL; /* No need to look for this one anymore. */
            args->nr_results++;

            /* Stop traversing if all fields were found. */
            if (args->nr_results == (ssize_t)args->nr_fields) {
                RedisModule_CloseKey(key);
                return 1;
            }
        } else if (err != SELVA_ENOENT) {
            /*
             * SELVA_ENOENT is expected as not all nodes have all fields set;
             * Any other error is unexpected.
             */
            fprintf(stderr, "%s:%d: Failed to get a field value. nodeId: %.*s fieldName: \"%s\" error: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    RedisModule_StringPtrLen(field_name, NULL),
                    getSelvaErrorStr(err));
        }
    }

    RedisModule_CloseKey(key);
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

    for (size_t i = 0; i < nr_field_names; i++) {
        RedisModuleString *field_name = field_names[i];
        TO_STR(field_name);
        int err;

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
int SelvaInheritCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
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
     * Run inherit for ancestors, descendants, parents, and children.
     * These fields will always exist in every node so these can be always
     * resolved at the top level.
     */
    size_t nr_presolved = inheritHierarchyFields(ctx, hierarchy, node_id, nr_types, types, nr_field_names, field_names);

    /*
     * Execute a traversal to inherit the requested field values.
     */
    struct InheritCommand_Args args = {
        .ctx = ctx,
        .hierarchy = hierarchy,
        .lang = lang,
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
        fprintf(stderr, "%s:%d: Inherit failed: %s\n",
                __FILE__, __LINE__,
                getSelvaErrorStr(err));
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
