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

void ring_buffer_init(struct ring_buffer* rb, struct ring_buffer_element *buf, size_t nelem, void (*free_element_data)(void *p, ring_buffer_eid_t eid))
{
    memset(rb, 0, sizeof(*rb));

    rb->tail = 0;
    rb->len = nelem;
    rb->buf = buf;
    rb->free_element_data = free_element_data;
    rb->readers_mask = ATOMIC_VAR_INIT(0);
    pthread_mutex_init(&rb->lock, NULL);
    pthread_cond_init(&rb->cond, NULL);

    for (size_t i = 0; i < rb->len; i++) {
        rb->buf[i].not_read = ATOMIC_VAR_INIT(0);
    }
}

int ring_buffer_init_state(struct ring_buffer_reader_state* state, struct ring_buffer* rb, ring_buffer_eid_t id, unsigned reader_id)
{
    const unsigned mask = 1 << reader_id;
    ssize_t new_index = -1;

    pthread_mutex_lock(&rb->lock);

    int j = (rb->tail) % rb->len;
    for (size_t i = 0; i < rb->len; i++) {
        if (rb->buf[j].id == id) {
            atomic_fetch_and(&rb->buf[j].not_read, ~mask);
            new_index = j;
        } else if (new_index >= 0) {
            atomic_fetch_or(&rb->buf[j].not_read, mask);
        }
        j = (j + 1) % rb->len;
    }

    pthread_mutex_unlock(&rb->lock);

    if (new_index == -1) {
        return 1;
    }

    state->index = new_index;
    state->reader_id = reader_id;

    return 0;
}

void ring_buffer_add_reader(struct ring_buffer *rb, unsigned reader_id)
{
    const unsigned mask = 1 << reader_id;

    atomic_fetch_or(&rb->readers_mask, mask);
}

void ring_buffer_del_readers_mask(struct ring_buffer *rb, unsigned readers)
{
    unsigned prev_readers;

    pthread_mutex_lock(&rb->lock);
    prev_readers = atomic_fetch_and(&rb->readers_mask, ~readers);
    pthread_mutex_unlock(&rb->lock);

    if (prev_readers & readers) {
        pthread_cond_broadcast(&rb->cond);
    }
}

void ring_buffer_del_reader(struct ring_buffer *rb, unsigned reader_id)
{
    const unsigned mask = 1 << reader_id;

    ring_buffer_del_readers_mask(rb, mask);
}

void ring_buffer_reader_exit(struct ring_buffer *rb, struct ring_buffer_reader_state *state)
{
    const unsigned mask = 1 << state->reader_id;

    ring_buffer_del_readers_mask(rb, mask);

    /*
     * Clear from the elements in the ring buffer.
     */
    for (size_t i = 0; i < rb->len; i++) {
        atomic_fetch_and(&rb->buf[i].not_read, ~mask);
    }
}

unsigned ring_buffer_insert(struct ring_buffer * restrict rb, ring_buffer_eid_t id, int8_t cmd_id, void * restrict p, size_t size)
{
    unsigned not_read;
    struct ring_buffer_element *e;
    typeof(e->id) old_id;
    typeof(e->data) old_data = NULL;

    /*
     * Locking the mutex here is important to invoke a proper memory barrier and
     * to avoid code reordering.
     * We also need to avoid a situation where a new replica is registered at
     * tail after the following atomic_load.
     */
    pthread_mutex_lock(&rb->lock);

    if ((not_read = atomic_load(&rb->buf[rb->tail].not_read))) {
        pthread_mutex_unlock(&rb->lock);
        return not_read;
    }

    e = &rb->buf[rb->tail];

    if (rb->free_element_data && e->data) {
        old_id = e->id;
        old_data = e->data;
    }

    e->id = id;
    e->cmd_id = cmd_id;
    e->data = p;
    e->data_size = size;
    atomic_store(&e->not_read, atomic_load(&rb->readers_mask));

    rb->tail = (rb->tail + 1) % rb->len;

    pthread_mutex_unlock(&rb->lock);
    pthread_cond_broadcast(&rb->cond);

    if (old_data) {
        rb->free_element_data(old_data, old_id);
    }

    return 0;
}

static int should_stop(struct ring_buffer *rb, unsigned mask)
{
    return !(atomic_load(&rb->readers_mask) & mask);
}

int ring_buffer_get_next(struct ring_buffer *rb, struct ring_buffer_reader_state *state, struct ring_buffer_element **e)
{
    const unsigned mask = 1 << state->reader_id;
    const size_t next = (state->index + 1) % rb->len;

    pthread_mutex_lock(&rb->lock);
    while (next == rb->tail && !should_stop(rb, mask)) {
        pthread_cond_wait(&rb->cond, &rb->lock);
    }
    pthread_mutex_unlock(&rb->lock);
    if (should_stop(rb, mask)) {
        return 0;
    }

    *e = &rb->buf[next];
    state->index = next;

    return 1;
}

void ring_buffer_release(struct ring_buffer_reader_state *state, struct ring_buffer_element *e)
{
    const unsigned mask = 1 << state->reader_id;

    atomic_fetch_and(&e->not_read, ~mask);
}
