/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include <sys/types.h>
#include "util/cstrings.h"
#include "selva_error.h"
#include "selva_db.h"
#include "selva_object.h"
#include "edge.h"
#include "hierarchy.h"
#include "inherit_fields.h"

static int get_field_value(
        SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_str,
        size_t field_len,
        struct SelvaObjectAny *out);

static int get_edge_field_value(struct EdgeField *edge_field, struct SelvaObjectAny *out) {
    (void)edge_field;
    (void)out;
    /* TODO Do we want to support this. */
#if 0
    out->type = SELVA_OBJECT_ARRAY;
    out->subtype = SELVA_OBJECT_POINTER;
    out->array = edge_field->arcs;
#endif

    return SELVA_EINVAL;
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

static int get_edge_field_deref_value(
        SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const struct EdgeField *edge_field,
        const char *field_str,
        size_t field_len,
        struct SelvaObjectAny *out) {
    struct SelvaObject *obj;
    Selva_NodeId nodeId;
    int err;

    err = deref_single_ref(edge_field, nodeId, &obj);
    if (err) {
        return err;
    }

    if (field_len == 1 && field_str[0] == '*') {
        out->type = SELVA_OBJECT_OBJECT;
        out->subtype = SELVA_OBJECT_NULL;
        out->obj = obj;
    } else {
        const struct SelvaHierarchyNode *node;

        node = SelvaHierarchy_FindNode(hierarchy, nodeId);
        if (!node) {
            return SELVA_ENOENT; /* RFE Should we return SELVA_HIERARCHY_ENOENT? */
        }

        return get_field_value(hierarchy, lang, node, obj, field_str, field_len, out);
    }

    return 0;
}

static int get_field_value(
        SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_str,
        size_t field_len,
        struct SelvaObjectAny *out) {
    struct EdgeField *edge_field;

    /*
     * If field is an edge field then the client wants to get the value of it,
     * usually an array of node ids.
     */
    edge_field = Edge_GetField(node, field_str, field_len);
    if (edge_field) {
        Selva_NodeId node_id;

        SelvaHierarchy_GetNodeId(node_id, node);

        return get_edge_field_value(edge_field, out);
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

                return get_edge_field_deref_value(hierarchy, lang, edge_field, rest_str, rest_len, out);
            }
        }
    }

    /* Finally try from a node object field. */
    return SelvaObject_GetAnyLangStr(obj, lang, field_str, field_len, out);
}

int Inherit_GetField(
        SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_str,
        size_t field_len,
        struct SelvaObjectAny *out) {
    return get_field_value(hierarchy, lang, node, obj, field_str, field_len, out);
}
