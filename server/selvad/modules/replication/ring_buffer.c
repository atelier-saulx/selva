/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h> /* Only needed because sometimes static_assert doesn't work yet */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include "ring_buffer.h"

void ring_buffer_init(struct ring_buffer* rb, void (*free_element_data)(void *p, ring_buffer_eid_t eid))
{
    memset(rb, 0, sizeof(*rb));

    rb->tail = 0;
    rb->len = RING_BUFFER_SIZE;
    rb->free_element_data = free_element_data;
    rb->replicas_mask = ATOMIC_VAR_INIT(0);
    pthread_mutex_init(&rb->lock, NULL);
    pthread_cond_init(&rb->cond, NULL);

    for (size_t i = 0; i < rb->len; i++) {
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
        pthread_cond_broadcast(&rb->cond);
    }
}

void ring_buffer_replica_exit(struct ring_buffer *rb, struct ring_buffer_reader_state *state)
{
    const unsigned mask = 1 << state->replica;

    /*
     * Clear from the elements in the ring buffer.
     */
    for (size_t i = 0; i < rb->len; i++) {
        atomic_fetch_and(&rb->buffer[i].not_replicated, ~mask);
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
        rb->free_element_data(e->data, e->id);
    }

    e->id = id;
    e->data = p;
    atomic_store(&e->not_replicated, atomic_load(&rb->replicas_mask));

    /*
     * Locking the mutex here is important to invoke a proper memory barrier and
     * to avoid code reordering.
     */
    pthread_mutex_lock(&rb->lock);
    rb->tail = (rb->tail + 1) % rb->len;
    pthread_mutex_unlock(&rb->lock);

    pthread_cond_broadcast(&rb->cond);
    return 0;
}

static int should_stop(struct ring_buffer *rb, unsigned mask)
{
    return !(atomic_load(&rb->replicas_mask) & mask);
}

int ring_buffer_get_next(struct ring_buffer *rb, struct ring_buffer_reader_state *state, struct ring_buffer_element **e)
{
    const unsigned mask = 1 << state->replica;
    const size_t next = (state->index + 1) % rb->len;

    pthread_mutex_lock(&rb->lock);
    while (next == rb->tail && !should_stop(rb, mask)) {
        pthread_cond_wait(&rb->cond, &rb->lock);
    }
    pthread_mutex_unlock(&rb->lock);
    if (should_stop(rb, mask)) {
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
