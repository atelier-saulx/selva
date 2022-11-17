/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _FIND_INDEX_PICK_ICB_
#define _FIND_INDEX_PICK_ICB_

/**
 * Pick an ICB.
 * 1. If first is valid then the function simply returns it;
 * 2. Otherwise, the function attempts for find a close match that's a super
 *    set of `desc` but doesn't necessarily have the same ordering as requested.
 * @parama first is a pointer to the exact match (ICB) to `desc` but not
 *               necessarily a valid index..
 * @returns a pointer to a potentially valid ICB.
 */
struct SelvaFindIndexControlBlock *SelvaFindIndexICB_Pick(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct icb_descriptor *desc,
        struct SelvaFindIndexControlBlock *first);

#endif /* _FIND_INDEX_PICK_ICB_ */
