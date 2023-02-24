/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <sys/types.h>
#include "util/selva_string.h"
#include "hierarchy.h"
#include "selva_object.h"
#include "selva_set.h"
#include "selva_set_ops.h"

struct sosfv_in_field {
    struct SelvaHierarchy *hierarchy;
    struct SelvaHierarchyNode *node;
    const char *field_b_str; /*!< Name of the set-like field. */
    size_t field_b_len;
    int valid; /*!< Is a valid subset. */
};

static int sosfv_in_field(union SelvaObjectSetForeachValue value, enum SelvaSetType type, void *arg) {
    struct sosfv_in_field *data = (struct sosfv_in_field *)arg;
    int has = 0;

    switch(type) {
    case SELVA_SET_TYPE_STRING:
        {
            size_t value_len;
            const char *value_str = selva_string_to_str(value.s, &value_len);

            has = SelvaSet_field_has_string(data->hierarchy, data->node, data->field_b_str, data->field_b_len, value_str, value_len);
        }
        break;
    case SELVA_SET_TYPE_DOUBLE:
        has = SelvaSet_field_has_double(data->hierarchy, data->node, data->field_b_str, data->field_b_len, value.d);
        break;
    case SELVA_SET_TYPE_LONGLONG:
        has = SelvaSet_field_has_longlong(data->hierarchy, data->node, data->field_b_str, data->field_b_len, value.ll);
        break;
    case SELVA_SET_TYPE_NODEID:
        has = SelvaSet_field_has_string(data->hierarchy, data->node, data->field_b_str, data->field_b_len, value.node_id, SELVA_NODE_ID_SIZE);
        break;
    case SELVA_SET_NR_TYPES:
        /* Invalid type? */
        break;
    }

    return !(data->valid = has);
}

int SelvaSet_fielda_in_fieldb(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_a_str,
        size_t field_a_len,
        const char *field_b_str,
        size_t field_b_len) {
    struct sosfv_in_field data = {
        .hierarchy = hierarchy,
        .node = node,
        .field_b_str = field_b_str,
        .field_b_len = field_b_len,
        .valid = 1,
    };
    struct SelvaObjectSetForeachCallback cb = {
        .cb = sosfv_in_field,
        .cb_arg = &data,
    };
    int err;

    err = SelvaHierarchy_ForeachInField(hierarchy, node, field_a_str, field_a_len, &cb);

    return err ? 0 : data.valid;
}