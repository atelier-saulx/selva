/*
 * Copyright (c) 2020-2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _UTIL_TRX_H_
#define _UTIL_TRX_H_

#include "cdefs.h"

typedef uint64_t trxid_t;

/**
 * Global transaction state.
 * A transaction is reentrant and the same transaction can call Trx_Begin()
 * multiple times. A transaction ends when Trx_End() is called the same number
 * of times as Trx_Begin() was called. The last call to Trx_End() initializes a
 * new transaction.
 */
struct trx_state {
    trxid_t id; /*!< Id of the transaction. */
    trxid_t cl; /*!< Colors used in the transaction. */
    trxid_t ex; /*!< Traversals that have finished. */
};

/**
 * Transaction label/element state.
 * When used to hold the current reentrant state the variable is typically
 * called trx_cur, while the label in the traversed data structure is
 * called trx_label.
 */
struct trx {
    trxid_t id; /*!< Id of the currently executing transaction. */
    trxid_t cl; /*!< Color of the currently executing traversal. */
};

/**
 * Start a new traversal.
 * This function will either start a new transaction or select a new color
 * for an open transaction.
 */
int Trx_Begin(struct trx_state * restrict state, struct trx * restrict trx);

/**
 * Sync the transaction id to the label.
 * This function is only useful if you want to update the latest transaction
 * id to the label but you don't need to know later on if the transaction
 * (traversal) actually visited the node.
 */
void Trx_Sync(const struct trx_state * restrict state, struct trx * restrict label);

/**
 * Visit a node.
 * @returns 0 if the node should not be visited;
 *          1 if the node should be visited.
 */
int __hot Trx_Visit(struct trx * restrict cur_trx, struct trx * restrict label);

/**
 * Test if cur_tx has visited label.
 */
int Trx_HasVisited(const struct trx * restrict cur_trx, const struct trx * restrict label);

/**
 * End traversal.
 */
void Trx_End(struct trx_state * restrict state, struct trx * restrict cur);

/**
 * Test if the transaction has finished.
 */
static inline int Trx_Fin(const struct trx_state * restrict state) {
    return state->cl == 0;
}

/**
 * Calculate the age of the given label.
 * The label age is practically a distance or a difference between the current
 * id and when the label was stamped with an id the last time.
 */
static inline long long Trx_LabelAge(const struct trx_state * restrict state, const struct trx * restrict label) {
    return (long long)(state->id - label->id);
}

#endif /* _UTIL_TRX_H_ */
