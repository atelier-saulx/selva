/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <sys/types.h>
#include "selva_db.h"
#include "hierarchy.h"
#include "selva_object.h"
#include "selva_set.h"
#include "selva_set_ops.h"

struct sosfv_in_set {
    struct SelvaSet *set;
    size_t found; /*! How many elements we have found from set. */
};

static int sosfv_in_set(union SelvaObjectSetForeachValue value, enum SelvaSetType type, void *arg) {
    struct sosfv_in_set *data = (struct sosfv_in_set *)arg;

    /*
     * Let n be the cardinality of seta. If we can find n elements from fieldb that
     * are also in seta, then seta is a subset of fieldb. This might result an
     * invalid result if the field is an array.
     */
    switch (type) {
    case SELVA_SET_TYPE_STRING:
        if (data->set->type == SELVA_SET_TYPE_STRING) {
            data->found += SelvaSet_Has(data->set, value.s);
        } else if (data->set->type == SELVA_SET_TYPE_NODEID) {
            struct SelvaSetElement *el;

            SELVA_SET_NODEID_FOREACH(el, data->set) {
                Selva_NodeId node_id;
                int err;

                err = selva_string2node_id(node_id, value.s);
                if (!err && !memcmp(node_id, el->value_nodeId, SELVA_NODE_ID_SIZE)) {
                    data->found++;
                    break;
                }
            }
        }
        break;
    case SELVA_SET_TYPE_DOUBLE:
        data->found += SelvaSet_Has(data->set, value.d);
        break;
    case SELVA_SET_TYPE_LONGLONG:
        data->found += SelvaSet_Has(data->set, value.ll);
        break;
    case SELVA_SET_TYPE_NODEID:
        if (data->set->type == SELVA_SET_TYPE_NODEID) {
            data->found += SelvaSet_Has(data->set, value.node_id);
        } else if (data->set->type == SELVA_SET_TYPE_STRING) {
            struct SelvaSetElement *el;

            SELVA_SET_STRING_FOREACH(el, data->set) {
                Selva_NodeId node_id;
                int err;

                err = selva_string2node_id(node_id, el->value_string);
                if (!err && !memcmp(node_id, value.node_id, SELVA_NODE_ID_SIZE)) {
                    data->found++;
                    break;
                }
            }
        }
        break;
    case SELVA_SET_NR_TYPES:
        /* Invalid type? */
        break;
    }

    return 0;
}

int SelvaSet_seta_in_fieldb(
        struct SelvaSet *a,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_b_str,
        size_t field_b_len) {
    struct sosfv_in_set data = {
        .found = 0,
        .set = a,
    };
    struct SelvaObjectSetForeachCallback cb = {
        .cb = sosfv_in_set,
        .cb_arg = &data,
    };
    int err;

    err = SelvaHierarchy_ForeachInField(hierarchy, node, field_b_str, field_b_len, &cb);

    return err ? 0 : data.found == SelvaSet_Size(a);
}
