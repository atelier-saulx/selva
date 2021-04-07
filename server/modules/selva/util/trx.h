#pragma once
#ifndef _UTIL_TRX_H_
#define _UTIL_TRX_H_

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
    trxid_t cl; /* Colors used in the transaction. */
    trxid_t ex; /* Traversals that have finished. */
};

/**
 * Transaction traversal state.
 */
struct trx {
    trxid_t id; /*!< Id of the currently executing transaction. */
    trxid_t cl; /*!< Color of the currently executing traversal. */
};

/**
 * Start a new traversal.
 */
int Trx_Begin(struct trx_state * restrict state, struct trx * restrict trx);

/**
 * Visit a node.
 */
int Trx_Visit(struct trx * restrict cur_trx, struct trx * restrict label);

/**
 * End traversal.
 */
void Trx_End(struct trx_state * restrict state, struct trx * restrict cur);

#endif /* _UTIL_TRX_H_ */
