/*
 * Copyright (c) 2021-2022 SAULX
 * SPDX-License-Identifier: (MIT WITH selva-exception) OR AGPL-3.0-only
 */
#include <stddef.h>
#include "base64.h"
#include "redismodule.h"
#include "svector.h"
#include "selva.h"
#include "selva_set.h"
#include "traversal.h"
#include "traversal_order.h"
#include "hierarchy.h"
#include "icb.h"

size_t SelvaFindIndexICB_CalcNameLen(const Selva_NodeId node_id, const struct icb_descriptor *desc) {
    size_t filter_len;
    size_t n;

    if (desc->filter) {
        (void)RedisModule_StringPtrLen(desc->filter, &filter_len);
    } else {
        filter_len = 0;
    }

    n = Selva_NodeIdLen(node_id) + base64_out_len(filter_len, 0) + 3;

    if (desc->dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
        size_t dir_expression_len;

        (void)RedisModule_StringPtrLen(desc->dir_expression, &dir_expression_len);

        /*
         * Currently only expressions are supported in addition to fixed
         * field name traversals.
         */
        n += base64_out_len(dir_expression_len, 0) + 1;
    }

    if (desc->sort.order != SELVA_RESULT_ORDER_NONE) {
        size_t order_field_len;

        (void)RedisModule_StringPtrLen(desc->sort.order_field, &order_field_len);

        n += base64_out_len(order_field_len, 0) + 3;
    }

    return n;
}

void SelvaFindIndexICB_BuildName(char *buf, const Selva_NodeId node_id, const struct icb_descriptor *desc) {
    size_t filter_len = 0;
    const char *filter_str = desc->filter ? RedisModule_StringPtrLen(desc->filter, &filter_len) : NULL;
    char *s = buf;

    /* node_id */
    memcpy(s, node_id, Selva_NodeIdLen(node_id));
    s += Selva_NodeIdLen(node_id);

    /* direction */
    *s++ = '.';
    *s++ = 'A' + (char)__builtin_ffs(desc->dir);

    if (desc->dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
        size_t dir_expression_len;
        const char *dir_expression_str = RedisModule_StringPtrLen(desc->dir_expression, &dir_expression_len);

        if (dir_expression_len > 0) {
            *s++ = '.';
            s += base64_encode_s(s, dir_expression_str, dir_expression_len, 0);
        }
    }

    /* order */
    if (desc->sort.order != SELVA_RESULT_ORDER_NONE) {
        size_t order_field_len;
        const char *order_field_str = RedisModule_StringPtrLen(desc->sort.order_field, &order_field_len);

        *s++ = '.';
        *s++ = 'A' + (char)desc->sort.order;
        *s++ = '.';
        s += base64_encode_s(s, order_field_str, order_field_len, 0); /* This must be always longer than 0 */
    }

    /*
     * indexing clause filter
     * Normally this is always set unless we want to specifically build a string without it.
     */
    if (filter_len > 0) {
        *s++ = '.';
        s += base64_encode_s(s, filter_str, filter_len, 0);
    }
}

int SelvaFindIndexICB_Get(struct SelvaHierarchy *hierarchy, const char *name_str, size_t name_len, struct SelvaFindIndexControlBlock **icb) {
    struct SelvaObject *dyn_index = hierarchy->dyn_index.index_map;
    void *p;
    int err;

    err = SelvaObject_GetPointerStr(dyn_index, name_str, name_len, &p);
    *icb = p;

    return err;
}

int SelvaFindIndexICB_Set(struct SelvaHierarchy *hierarchy, const char *name_str, size_t name_len, struct SelvaFindIndexControlBlock *icb) {
    struct SelvaObject *dyn_index = hierarchy->dyn_index.index_map;

    return SelvaObject_SetPointerStr(dyn_index, name_str, name_len, icb, NULL);
}

int SelvaFindIndexICB_Del(struct SelvaHierarchy *hierarchy, const struct SelvaFindIndexControlBlock *icb) {
    return SelvaObject_DelKeyStr(hierarchy->dyn_index.index_map, icb->name_str, icb->name_len);
}
