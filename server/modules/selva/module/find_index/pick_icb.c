/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: (MIT WITH selva-exception) OR AGPL-3.0-only
 */
#include <stddef.h>
#include "redismodule.h"
#include "svector.h"
#include "selva.h"
#include "traversal.h"
#include "traversal_order.h"
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
        /* Fallback to unordered index. */
        struct SelvaFindIndexControlBlock *icb_unord;

        icb_unord = pick_unordered(hierarchy, node_id, desc);
        if (!icb || (icb_unord && icb_unord->flags.valid)) {
            icb = icb_unord;
        }

        /*
         * TODO As find allows us to return any order in addition to the requested,
         * we should try to check if we have anything matching the query in case
         * icb_unord was not valid.
         */
    } else if (order == SELVA_RESULT_ORDER_NONE) {
        /*
         * TODO
         * Unordered find query (doesn't support $limit with indexing) can accept
         * any order. Try to find any valid index with the right dir and clause.
         */
    }

    return icb;
}
