/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include "module.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_server.h"
#include "ring_buffer.h"
#include "replica.h"

/*
 * TODO We need a global config for core mapping.
 */
static int thread_set_self_core(int core_id)
{
    const pthread_t thread = pthread_self();
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset)) {
        /* TODO Better error handling. */
        return SELVA_EGENERAL;
    }

    return 0;
}

static void log_exit(struct selva_server_response_out *resp)
{
    char strcon[80];
    size_t len;

    len = selva_resp_to_str(resp, strcon, sizeof(strcon));
    SELVA_LOG(SELVA_LOGL_INFO, "Replica going offline (%.*s)", (int)len, strcon);
}

void *replication_thread(void *arg)
{
    struct replica *replica = (struct replica *)arg;
    struct selva_server_response_out *resp = replica->resp;
    struct ring_buffer *rb = replica->rb;
    struct ring_buffer_reader_state state;
    struct ring_buffer_element *e;

    if (ring_buffer_init_state(&state, rb, replica->start_eid, replica->id)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to initialize a ring_buffer_reader_state");
        goto out;
    }

    /* TODO re-enable later */
#if 0
    thread_set_self_core(replica->core_id);
#endif

    while (ring_buffer_get_next(rb, &state, &e)) {
        ssize_t res;

        res = selva_send_buf(resp, e->data, e->data_size);
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
