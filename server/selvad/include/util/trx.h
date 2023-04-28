/*
 * Copyright (c) 2020-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _UTIL_TRX_H_
#define _UTIL_TRX_H_

#include "cdefs.h"

typedef uint64_t trxid_t;

/**
 * Global transaction state.
 * There should be one of these structures once per the whole data structure
 * to manage the transaction state system.
 * A transaction is reentrant and the same transaction can call Trx_Begin()
 * multiple times. A transaction ends when Trx_End() is called the same number
 * of times as Trx_Begin() was called. The last call to Trx_End() initializes a
 * new transaction.
 */
struct trx_state {
    trxid_t id; /*!< Id of the currently executing transaction. */
    trxid_t cl; /*!< Traversal colors used in the transaction. */
    trxid_t ex; /*!< Traversal colors that have finished in this transaction. */
};

/**
 * Transaction label/element state.
 * Every element or node in a data structure should have a label structure of
 * this type.
 */
struct trx_label {
    trxid_t id; /*!< Id of the currently executing transaction. */
    trxid_t cl; /*!< Color of the currently executing traversal. */
};

/**
 * Current transaction state.
 * Holds the state of the current transaction. This structure is typically
 * allocated as a stack variable called trx_cur.
 */
struct trx {
    trxid_t id; /*!< Id of the currently executing transaction. */
    trxid_t cl; /*!< Color of the currently executing traversal. */
};

/**
 * Start a new traversal.
 * This function will either start a new transaction or select a new color
 * for a transaction currently open.
 */
int Trx_Begin(struct trx_state * restrict state, struct trx * restrict trx);

/**
 * Sync the transaction id to the label.
 * This function is only useful if you want to update the latest transaction
 * id to the label but you don't need to know later on if the transaction
 * (traversal) actually visited the node.
 */
void Trx_Sync(const struct trx_state * restrict state, struct trx_label * restrict label);

/**
 * Attempt to visit a transaction label.
 * @returns 0 if the node should not be visited;
 *          1 if the node should be visited.
 */
int __hot Trx_Visit(struct trx * restrict cur_trx, struct trx_label * restrict label);

/**
 * Test if cur_tx has visited label.
 */
int Trx_HasVisited(const struct trx * restrict cur_trx, const struct trx_label * restrict label);

/**
 * End the current traversal.
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
static inline long long Trx_LabelAge(const struct trx_state * restrict state, const struct trx_label * restrict label) {
    return (long long)(state->id - label->id);
}

#endif /* _UTIL_TRX_H_ */
