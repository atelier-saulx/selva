/*
 * Copyright (c) 2021-2022 SAULX
 * SPDX-License-Identifier: (MIT WITH selva-exception) OR AGPL-3.0-only
 */
#pragma once
#ifndef _FIND_INDEX_H_
#define _FIND_INDEX_H_

#include "traversal.h"
#include "traversal_order.h"

struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaHierarchy;
struct SelvaSet;
struct SelvaFindIndexControlBlock;
struct indexing_timer_args;

/**
 * Initialize a new indexing subsystem instance for hierarchy.
 */
void SelvaFindIndex_Init(struct RedisModuleCtx *ctx, struct SelvaHierarchy *hierarchy);

/**
 * Deinit the indexing subsystem instance initialized for hierarchy.
 */
void SelvaFindIndex_Deinit(struct SelvaHierarchy *hierarchy);

size_t SelvaFindIndex_IcbCard(const struct SelvaFindIndexControlBlock *icb);

/**
 * Check if an index exists for this query, update it, and get the indexing result set.
 * @param order Set to other than SELVA_RESULT_ORDER_NONE if the index should be sorted.
 * @param order_field Should be non-NULL only if the index should be sorted.
 * @param out is a SelvaSet of node_ids indexed for given clause.
 */
int SelvaFindIndex_Auto(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        enum SelvaTraversal dir, struct RedisModuleString *dir_expression_str,
        const Selva_NodeId node_id,
        enum SelvaResultOrder order,
        struct RedisModuleString *order_field,
        struct RedisModuleString *filter,
        struct SelvaFindIndexControlBlock **icb_out);
int SelvaFindIndex_AutoMulti(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        enum SelvaTraversal dir, struct RedisModuleString *dir_expression,
        const Selva_NodeId node_id,
        enum SelvaResultOrder order,
        struct RedisModuleString *order_field,
        struct RedisModuleString *index_hints[],
        size_t nr_index_hints,
        struct SelvaFindIndexControlBlock *ind_icb_out[]);

/**
 * Check whether an ICB is created as an ordered.
 * This function doesn't check whether the index is actually valid.
 */
int SelvaFindIndex_IsOrdered(
        struct SelvaFindIndexControlBlock *icb,
        enum SelvaResultOrder order,
        struct RedisModuleString *order_field);

int SelvaFindIndex_Traverse(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaFindIndexControlBlock *icb,
        SelvaHierarchyNodeCallback node_cb,
        void *node_arg);

/**
 * Update indexing accounting.
 * @param acc_take is the number of nodes taken from the original set.
 * @param acc_tot is the total number of nodes in the original set.
 */
void SelvaFindIndex_Acc(
        struct SelvaFindIndexControlBlock * restrict icb,
        size_t acc_take,
        size_t acc_tot);
void SelvaFindIndex_AccMulti(
        struct SelvaFindIndexControlBlock *ind_icb[],
        size_t nr_index_hints,
        int ind_select,
        size_t acc_take,
        size_t acc_tot);

#endif /* _FIND_INDEX_H_ */
