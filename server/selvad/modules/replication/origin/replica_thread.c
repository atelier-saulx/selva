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
#include "ring_buffer.h"
#include "replica.h"

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
    struct ring_buffer *rb = replica->rb;
    struct ring_buffer_reader_state state;
    struct ring_buffer_element *e;

    if (ring_buffer_init_state(&state, rb, 1, replica->id)) {
        goto out;
    }

    thread_set_self_core(replica->core_id);

    while (ring_buffer_get_next(rb, &state, &e)) {
        /* TODO Send to the client */
        /* TODO Abort if the client disconnects */
        ring_buffer_release(&state, e);
    }

out:
    ring_buffer_reader_exit(rb, replica->id);
    return NULL;
}
