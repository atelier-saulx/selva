/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include "util/cstrings.h"
#include "util/selva_string.h"
#include "selva_server.h"
#include "selva_error.h"
#include "selva_db.h"
#include "selva_object.h"
#include "edge.h"
#include "hierarchy.h"
#include "inherit_fields.h"

/*
 * Send field in the inherit command style:
 * [node_id, field_name, field_value]
 * i.e. Each response is encapsulated in an array.
 */

static int send_field_value(
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *full_field_str,
        size_t full_field_len,
        const char *field_str,
        size_t field_len);

static int send_edge_field_value(
        struct selva_server_response_out *resp,
        const Selva_NodeId node_id,
        const char *full_field_str,
        size_t full_field_len,
        struct EdgeField *edge_field) {
    selva_send_array(resp, 3);
    selva_send_str(resp, node_id, Selva_NodeIdLen(node_id));
    selva_send_str(resp, full_field_str, full_field_len);
    replyWithEdgeField(resp, edge_field);

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
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const char *full_field_str,
        size_t full_field_len,
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
        selva_send_array(resp, 3);
        selva_send_str(resp, nodeId, Selva_NodeIdLen(nodeId)); /* The actual node_id. */
        selva_send_str(resp, full_field_str, full_field_len - 2); /* -2 to remove the `.*` suffix */
        SelvaObject_ReplyWithObject(resp, lang, obj, NULL, 0);
    } else {
        const struct SelvaHierarchyNode *node;

        node = SelvaHierarchy_FindNode(hierarchy, nodeId);
        if (!node) {
            return SELVA_ENOENT; /* RFE Should we return SELVA_HIERARCHY_ENOENT? */
        }

        return send_field_value(resp, hierarchy, lang, node, obj, full_field_str, full_field_len, field_str, field_len);
    }

    return 0;
}

static int send_object_field_value(
        struct selva_server_response_out *resp,
        struct selva_string *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *full_field_str,
        size_t full_field_len,
        const char *field_str,
        size_t field_len) {
    int err = SELVA_ENOENT;

    if (!SelvaObject_ExistsStr(obj, field_str, field_len)) {
        Selva_NodeId node_id;

        SelvaHierarchy_GetNodeId(node_id, node);

        selva_send_array(resp, 3);
        selva_send_str(resp, node_id, Selva_NodeIdLen(node_id));
        selva_send_str(resp, full_field_str, full_field_len);

        err = SelvaObject_ReplyWithObjectStr(resp, lang, obj, field_str, field_len, 0);
        if (err) {
            (void)selva_send_errorf(resp, err, "failed to inherit field: \"%.*s\"", (int)field_len, field_str);
        }
    }

    return err;
}

static int send_field_value(
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *full_field_str,
        size_t full_field_len,
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

        return send_edge_field_value(resp, node_id, full_field_str, full_field_len, edge_field);
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

                return send_edge_field_deref_value(resp, hierarchy, lang, full_field_str, full_field_len, edge_field, rest_str, rest_len);
            }
        }
    }

    /* Finally try from a node object field. */
    return send_object_field_value(resp, lang, node, obj, full_field_str, full_field_len, field_str, field_len);
}

int Inherit_SendField(
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *full_field_str,
        size_t full_field_len,
        const char *field_str,
        size_t field_len) {
    return send_field_value(resp, hierarchy, lang, node, obj, full_field_str, full_field_len, field_str, field_len);
}
