/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "module.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_server.h"
#include "selva_replication.h"
#include "../selva_thread.h"
#include "ring_buffer.h"
#include "../replication.h"
#include "replica.h"

static void log_exit(struct selva_server_response_out *resp)
{
    char strcon[80];
    size_t len;

    len = selva_resp_to_str(resp, strcon, sizeof(strcon));
    SELVA_LOG(SELVA_LOGL_INFO, "Replica going offline (%.*s)", (int)len, strcon);
}

/**
 * Send the initial dump to the replica.
 * @param no_send can be set to indicate that the replica is already running on this sdb and it doesn't need to be sent again.
 */
static int sync_dump(
        struct selva_server_response_out *resp,
        struct ring_buffer *rb,
        struct ring_buffer_reader_state *state,
        const uint8_t expected_sdb_hash[SELVA_IO_HASH_SIZE],
        enum replication_sync_mode sync_mode)
{
    struct ring_buffer_element *e;
    struct selva_replication_sdb *sdb;
    int err;

    ring_buffer_get_current(rb, state, &e);
    if (!(e->id & EID_MSB_MASK)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Expected an SDB dump. eid: 0x%" PRIx64, e->id);
        err = SELVA_EINVAL;
        goto fail;
    }

    assert(e->data_size == sizeof(struct selva_replication_sdb));
    sdb = (struct selva_replication_sdb *)e->data;

    if (sdb->status != SDB_STATUS_COMPLETE) {
        SELVA_LOG(SELVA_LOGL_ERR, "Invalid or incomplete SDB structure. eid: 0x%" PRIx64, e->id);
        err = SELVA_EINVAL;
        goto fail;
    }

    if (memcmp(sdb->hash, expected_sdb_hash, SELVA_IO_HASH_SIZE)) {
        err = SELVA_ENOENT;
        goto fail;
    }

    if (sync_mode == REPLICATION_SYNC_MODE_FULL) {
        SELVA_LOG(SELVA_LOGL_INFO, "Full sync");
        err = selva_send_replication_sdb(resp, e->id, selva_string_to_str(sdb->filename, NULL));
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to sync replica's initial state: %s", selva_strerror(err));
            goto fail;
        }
    } else {
        SELVA_LOG(SELVA_LOGL_INFO, "Partial sync");
    }

    err = 0;
fail:
    ring_buffer_release(state, e);
    return err;
}

void *replication_thread(void *arg)
{
    struct replica *replica = (struct replica *)arg;
    struct selva_server_response_out *resp = replica->resp;
    struct ring_buffer *rb = replica->rb;
    struct ring_buffer_reader_state state;
    struct ring_buffer_element *e;

    SELVA_LOG(SELVA_LOGL_INFO, "Thread started");

    /*
     * RFE It would be better to be able to retry with another sdb eid instead
     * of letting the replica die but currently there is no easy way to do it.
     */
    if (ring_buffer_init_state(&state, rb, replica->start_eid, replica->id)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to initialize a ring_buffer_reader_state");
        selva_send_errorf(resp, SELVA_ENOENT, "Initial state mismatch");
        goto out;
    }

    /* TODO re-enable later */
#if 0
    selva_thread_set_self_core(replica->core_id);
#endif

    if (sync_dump(resp, rb, &state, replica->start_sdb_hash, replica->sync_mode)) {
        goto out;
    }

    while (ring_buffer_get_next(rb, &state, &e)) {
        int res;

        if (e->id & EID_MSB_MASK) {
            /*
             * This is a bit opportunistic because we don't know whether
             * incomplete sdb structs will ever be completed.
             */
            res = selva_send_replication_pseudo_sdb(resp, e->id);
        } else {
            res = selva_send_replication_cmd(resp, e->id, e->ts, e->cmd_id, e->data, e->data_size);
        }

        if (res < 0) {
            break;
        }

        if (selva_send_flush(resp)) {
            break;
        }

        ring_buffer_release(&state, e);
    }

out:
    log_exit(resp);
    selva_send_end(resp);
    ring_buffer_reader_exit(rb, &state);
    return NULL;
}
