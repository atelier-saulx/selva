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

void *replication_thread(void *arg)
{
    struct replica *replica = (struct replica *)arg;
    struct selva_server_response_out *resp = replica->resp;
    struct ring_buffer *rb = replica->rb;
    struct ring_buffer_reader_state state;
    struct ring_buffer_element *e;

    if (ring_buffer_init_state(&state, rb, 1, replica->id)) {
        goto out;
    }

    /* TODO re-enable later */
#if 0
    thread_set_self_core(replica->core_id);
#endif

    while (ring_buffer_get_next(rb, &state, &e)) {
        ssize_t res;
        int err;

        res = selva_send_buf(resp, e->data, e->data_size);
        if (res < 0) {
            /* TODO Log? */
            break;
        }

        err = selva_send_flush(resp);
        if (err) {
            /* TODO Log? */
            break;
        }

        ring_buffer_release(&state, e);
    }

out:
    ring_buffer_reader_exit(rb, &state);
    return NULL;
}
