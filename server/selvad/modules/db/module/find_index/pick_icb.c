/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include "base64.h"
#include "redismodule.h"
#include "svector.h"
#include "selva.h"
#include "traversal.h"
#include "hierarchy.h"
#include "icb.h"

/**
 * Pick a valid unordered ICB.
 */
static struct SelvaFindIndexControlBlock *pick_unordered(struct SelvaHierarchy *hierarchy, const Selva_NodeId node_id, const struct icb_descriptor *desc) {
    struct icb_descriptor icb_desc;
    struct SelvaFindIndexControlBlock *icb_unord = NULL;
    int err;

    memcpy(&icb_desc, desc, sizeof(icb_desc));
    icb_desc.sort.order = SELVA_RESULT_ORDER_NONE;
    icb_desc.sort.order_field = NULL;

    const size_t name_len = SelvaFindIndexICB_CalcNameLen(node_id, &icb_desc);
    char name_str[name_len];

    SelvaFindIndexICB_BuildName(name_str, node_id, &icb_desc);
    err = SelvaFindIndexICB_Get(hierarchy, name_str, name_len, &icb_unord);

    return err ? NULL : icb_unord;
}

/**
 * Recurse into the `<node_id>.<dir>[.<dir_expr>]` object until we find something.
 */
void pick_any_order_recursive(
        struct SelvaObject *obj,
        const char *base64_filter_str,
        size_t base64_filter_len,
        struct SelvaFindIndexControlBlock **out) {
    SelvaObject_Iterator *it;
    const char *name_str;
    enum SelvaObjectType type;
    void *p;

    it = SelvaObject_ForeachBegin(obj);
    while ((p = SelvaObject_ForeachValueType(obj, &it, &name_str, &type))) {
        if (type == SELVA_OBJECT_POINTER) {
            struct SelvaFindIndexControlBlock *icb = (struct SelvaFindIndexControlBlock *)p;
            const size_t name_len = strlen(name_str);

            if (base64_filter_len == name_len &&
                icb->flags.valid &&
                !memcmp(base64_filter_str, name_str, name_len)) {
                *out = icb;
                break;
            }
        } else if (type == SELVA_OBJECT_OBJECT) {
            pick_any_order_recursive(p, base64_filter_str, base64_filter_len, out);
            if (*out) {
                break;
            }
        }
    }
}

/**
 * Pick any ordered ICB.
 * Requirements:
 * - node_id matches to `desc`,
 * - traverse direction direction matches to `desc`,
 * - filter matches to `desc`,
 * - and finally, the index is valid.
 */
static struct SelvaFindIndexControlBlock *pick_any_order(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct icb_descriptor *desc) {
    struct icb_descriptor icb_desc;
    int err;

    memcpy(&icb_desc, desc, sizeof(icb_desc));
    icb_desc.sort.order = SELVA_RESULT_ORDER_NONE;
    icb_desc.sort.order_field = NULL;
    icb_desc.filter = NULL; /* build a name without the filter part. */

    const size_t name_len = SelvaFindIndexICB_CalcNameLen(node_id, &icb_desc);
    char name_str[name_len];

    SelvaFindIndexICB_BuildName(name_str, node_id, &icb_desc);

    struct SelvaObject *root;

    /* This kinda should be in icb.c */
    err = SelvaObject_GetObjectStr(hierarchy->dyn_index.index_map, name_str, name_len, &root);
    if (err) {
        return NULL; /* nada */
    }

    size_t filter_len;
    const char *filter_str = RedisModule_StringPtrLen(desc->filter, &filter_len);
    const size_t base64_filter_len = base64_out_len(filter_len, 0);
    char base64_filter_str[base64_filter_len];
    struct SelvaFindIndexControlBlock *out = NULL;

    /* This kinda should be in icb.c */
    base64_encode_s(base64_filter_str, filter_str, filter_len, 0);

    pick_any_order_recursive(root, base64_filter_str, base64_filter_len, &out);

    return out;
}

struct SelvaFindIndexControlBlock *SelvaFindIndexICB_Pick(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct icb_descriptor *desc,
        struct SelvaFindIndexControlBlock *first) {
    struct SelvaFindIndexControlBlock *icb = first;

    if (icb && icb->flags.valid) {
        /* First prefer requested index if it's valid. */
        return icb;
    }

    const enum SelvaResultOrder order = desc->sort.order;
    if (order != SELVA_RESULT_ORDER_NONE) {
        /* Fallback to an unordered index. */
        struct SelvaFindIndexControlBlock *alt_icb;

        alt_icb = pick_unordered(hierarchy, node_id, desc);
        if (!icb || (alt_icb && alt_icb->flags.valid)) {
            icb = alt_icb;
        }

        /*
         * As find allows us to return any order in addition to the requested
         * order, we can try to anything matching the query.
         */
        alt_icb = pick_any_order(hierarchy, node_id, desc);
        if (alt_icb && alt_icb->flags.valid) {
            icb = alt_icb;
        }
    }

    return icb;
}
