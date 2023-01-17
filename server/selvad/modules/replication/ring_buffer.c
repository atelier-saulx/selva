/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include "ring_buffer.h"

void ring_buffer_init(struct ring_buffer* rb, void (*free_element_data)(void *p))
{
    memset(rb, 0, sizeof(*rb));

    rb->tail = 0;
    rb->len = 1;
    rb->free_element_data = free_element_data;
    rb->replicas_mask = ATOMIC_VAR_INIT(0);
    pthread_mutex_init(&rb->lock, NULL);
    pthread_cond_init(&rb->cond, NULL);

    for (size_t i = 0; i < RING_BUFFER_SIZE; i++) {
        rb->buffer[i].not_replicated = ATOMIC_VAR_INIT(0);
    }
}

int ring_buffer_init_state(struct ring_buffer_reader_state* state, struct ring_buffer* rb, ring_buffer_eid_t id, unsigned replica_id)
{
    const unsigned mask = 1 << replica_id;
    ssize_t new_index = -1;

    pthread_mutex_lock(&rb->lock);

    int j = (rb->tail) % rb->len;
    for (size_t i = 0; i < rb->len; i++) {
        if (rb->buffer[j].id == id) {
            atomic_fetch_and(&rb->buffer[j].not_replicated, ~mask);
            new_index = j;
        } else if (new_index >= 0) {
            atomic_fetch_or(&rb->buffer[j].not_replicated, mask);
        }
        j = (j + 1) % rb->len;
    }

    if (new_index == -1) {
        pthread_mutex_unlock(&rb->lock);
        return 1;
    }

    state->index = new_index;
    state->replica = replica_id;

    pthread_mutex_unlock(&rb->lock);
    return 0;
}

void ring_buffer_add_replica(struct ring_buffer *rb, unsigned replica_id)
{
    const unsigned mask = 1 << replica_id;

    atomic_fetch_or(&rb->replicas_mask, mask);
}

void ring_buffer_del_replica(struct ring_buffer *rb, unsigned replica_id)
{
    const unsigned mask = 1 << replica_id;
    unsigned prev_replicas;

    prev_replicas = atomic_fetch_and(&rb->replicas_mask, ~mask);

    if (prev_replicas & mask) {
        /*
         * Clear from the elements in the ring buffer.
         */
        for (size_t i = 0; i < rb->len; i++) {
            atomic_fetch_and(&rb->buffer[i].not_replicated, ~mask);
        }

        /*
         * Wakeup the threads in case we weren't called by the replica
         * thread itself, as we must let it know that it must stop running
         * the replication.
         */
        pthread_cond_broadcast(&rb->cond);
    }
}

void ring_buffer_del_replica_mask(struct ring_buffer *rb, unsigned replicas)
{
    unsigned replica;

    while ((replica = __builtin_ffs(replicas))) {
        replica--;
        ring_buffer_del_replica(rb, replica);
        replicas ^= 1 << replica;
    }
}

unsigned ring_buffer_insert(struct ring_buffer * restrict rb, ring_buffer_eid_t id, void * restrict p)
{
    unsigned not_replicated = atomic_load(&rb->buffer[rb->tail].not_replicated);
    struct ring_buffer_element *e = &rb->buffer[rb->tail];

    if (atomic_load(&rb->buffer[rb->tail].not_replicated)) {
        return not_replicated;
    }

    if (rb->free_element_data && e->data) {
        rb->free_element_data(e->data);
    }

    e->id = id;
    e->data = p;
    atomic_store(&e->not_replicated, atomic_load(&rb->replicas_mask));
    if (rb->len < RING_BUFFER_SIZE) {
        rb->len++;
    }
    rb->tail = (rb->tail + 1) % rb->len;

    pthread_cond_broadcast(&rb->cond);
    return 0;
}

int ring_buffer_get_next(struct ring_buffer *rb, struct ring_buffer_reader_state *state, struct ring_buffer_element **e)
{
    const unsigned mask = 1 << state->replica;
    size_t next;
    int stop;

    pthread_mutex_lock(&rb->lock);
    while ((next = (state->index + 1) % rb->len) == rb->tail ||
           (stop = !(atomic_load(&rb->replicas_mask) & mask))) {
        if (stop) {
            break;
        }
        pthread_cond_wait(&rb->cond, &rb->lock);
    }
    pthread_mutex_unlock(&rb->lock);
    if (stop) {
        /*
         * We should stop the replication.
         */
        return 0;
    }

    *e = &rb->buffer[next];
    state->index = next;

    return 1;
}

void ring_buffer_mark_replicated(struct ring_buffer_reader_state *state, struct ring_buffer_element *e)
{
    const unsigned mask = 1 << state->replica;

    atomic_fetch_and(&e->not_replicated, ~mask);
}

#if 0
struct thread_args {
    int tid;
    struct ring_buffer* rb;
};

static void *thread_func(void *arg)
{
    struct thread_args *args = (struct thread_args *)arg;
    struct ring_buffer* rb = args->rb;
    int tid = args->tid;
    struct ring_buffer_reader_state state;
    struct ring_buffer_element *e;

    if (ring_buffer_init_state(&state, rb, 1, tid)) {
        goto out;
    }

    while (ring_buffer_get_next(rb, &state, &e)) {
        ...
        ring_buffer_mark_replicated(&state, e);
    }

out:
    ring_buffer_del_replica(rb, tid);
    return NULL;
}

int main(void)
{
    struct ring_buffer rb;
    struct thread_args args[2] = {
        {
            .tid = 0,
            .rb = &rb,
        },
        {
            .tid = 1,
            .rb = &rb,
        }
    };
    pthread_t thread1, thread2;

    ring_buffer_init(&rb);
    ring_buffer_add_replica(&rb, 0);
    ring_buffer_add_replica(&rb, 1);
    (void)ring_buffer_insert(&rb, 1, NULL);
    pthread_create(&thread1, NULL, thread_func, &args[0]);
    pthread_create(&thread2, NULL, thread_func, &args[1]);

    for (ring_buffer_eid_t i = 2; i <= TEST_LAST; i++) {
        unsigned not_replicated;

        while ((not_replicated = ring_buffer_insert(&rb, i, NULL))) {
            ring_buffer_del_replica_mask(&rb, not_replicated);
        }

        sleep(1);
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}
#endif
