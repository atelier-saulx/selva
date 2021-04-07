#include <stdint.h>
#include "trx.h"

int Trx_Begin(struct trx_state * restrict state, struct trx * restrict trx) {
    const trxid_t cl = (trxid_t)1 << __builtin_popcount(state->cl);

    if (cl == (trxid_t)1 << (sizeof(trxid_t) * 8 - 1)) {
        return -1;
    }

    state->cl |= cl;

    trx->id = state->id;
    trx->cl = cl;

    return 0;
}

int Trx_Visit(struct trx * restrict cur_trx, struct trx * restrict label) {
    if (cur_trx->id != label->id) {
        /* Visit. */
        label->id = cur_trx->id;
        label->cl = cur_trx->cl;

        return 1;
    } else if (!(cur_trx->cl & label->cl)) {
        /* Visit */
        label->id = cur_trx->id;
        label->cl |= cur_trx->cl;

        return 1;
    }

    return 0; /* Don't visit. */
}

void Trx_End(struct trx_state * restrict state, struct trx * restrict cur) {
    state->ex |= cur->cl;

    if (state->ex == state->cl) {
        state->id++;
        state->cl = 0;
        state->ex = 0;
    }
}
