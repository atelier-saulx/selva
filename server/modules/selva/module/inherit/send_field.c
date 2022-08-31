#include <string.h>
#include "redismodule.h"
#include "cstrings.h"
#include "selva.h"
#include "errors.h"
#include "selva_object.h"
#include "edge.h"
#include "hierarchy.h"
#include "inherit_fields.h"

static int send_field_value(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        RedisModuleString *full_field,
        const char *field_str,
        size_t field_len);

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
        const struct EdgeField *edge_field,
        Selva_NodeId node_id_out,
        struct SelvaObject **obj_out) {
    struct SelvaHierarchyNode *node;
    int err;

    err = Edge_DerefSingleRef(edge_field, &node);
    if (err) {
        return err;
    }

    SelvaHierarchy_GetNodeId(node_id_out, node);
    *obj_out = SelvaHierarchy_GetNodeObject(node);
    return 0;
}

static int send_edge_field_deref_value(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
        RedisModuleString *full_field,
        const struct EdgeField *edge_field,
        const char *field_str,
        size_t field_len) {
    struct SelvaObject *obj;
    Selva_NodeId nodeId;
    int err;

    err = deref_single_ref(edge_field, nodeId, &obj);
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
        SelvaObject_ReplyWithObject(ctx, lang, obj, NULL, SELVA_OBJECT_REPLY_BINUMF_FLAG);
    } else {
        const struct SelvaHierarchyNode *node;

        node = SelvaHierarchy_FindNode(hierarchy, nodeId);
        if (!node) {
            return SELVA_ENOENT; /* RFE Should we return SELVA_HIERARCHY_ENOENT? */
        }

        return send_field_value(ctx, hierarchy, lang, node, obj, full_field, field_str, field_len);
    }

    return 0;
}

static int send_object_field_value(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        RedisModuleString *full_field,
        const char *field_str,
        size_t field_len) {
    int err = SELVA_ENOENT;

    if (!SelvaObject_ExistsStr(obj, field_str, field_len)) {
        Selva_NodeId node_id;

        SelvaHierarchy_GetNodeId(node_id, node);

        /*
         * Start a new array reply:
         * [node_id, field_name, field_value]
         */
        RedisModule_ReplyWithArray(ctx, 3);
        RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));
        RedisModule_ReplyWithString(ctx, full_field);

        err = SelvaObject_ReplyWithObjectStr(ctx, lang, obj, field_str, field_len, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err) {
            (void)replyWithSelvaErrorf(ctx, err, "failed to inherit field: \"%.*s\"", (int)field_len, field_str);
        }
    }

    return err;
}

static int send_field_value(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
        const struct SelvaHierarchyNode *node,
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
        Selva_NodeId node_id;

        SelvaHierarchy_GetNodeId(node_id, node);

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
    return send_object_field_value(ctx, lang, node, obj, full_field, field_str, field_len);
}

int Inherit_SendField(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        RedisModuleString *full_field,
        const char *field_str,
        size_t field_len) {
    return send_field_value(ctx, hierarchy, lang, node, obj, full_field, field_str, field_len);
}
