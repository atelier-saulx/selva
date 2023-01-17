/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#define RING_BUFFER_SIZE 5

typedef int64_t ring_buffer_eid_t; /*!< Element id type. */

/*
 * We use atomic_uint to access replica masks. Otherwise replica ids and masks
 * are passed using the unsigned type.
 * We assume to have at least 32 replica ids available.
 */
static_assert(sizeof(atomic_uint) >= sizeof(unsigned));
static_assert(sizeof(unsigned) >= sizeof(uint32_t));

struct ring_buffer_element {
    ring_buffer_eid_t id;
    atomic_uint not_replicated;
    void *data;
};

struct ring_buffer {
    struct ring_buffer_element buffer[RING_BUFFER_SIZE];
    size_t tail;
    size_t len;
    void (*free_element_data)(void *p);
    atomic_uint replicas_mask;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

struct ring_buffer_reader_state {
    size_t index;
    ring_buffer_eid_t next_id;
    unsigned replica;
};

void ring_buffer_init(struct ring_buffer* rb, void (*free_element_data)(void *p));
int ring_buffer_init_state(struct ring_buffer_reader_state* state, struct ring_buffer* rb, ring_buffer_eid_t id, unsigned replica_id);

/**
 * Add a replica to the ring_buffer.
 * Must be called by the writer before the replica thread is started.
 */
void ring_buffer_add_replica(struct ring_buffer *rb, unsigned replica_id);

/**
 * Delete a replica from the ring_buffer.
 * Can be called by the replication thead or the writer.
 */
void ring_buffer_del_replica(struct ring_buffer *rb, unsigned replica_id);

/**
 * Calls ring_buffer_del_replica() for each replica in the replicas bit mask.
 */
void ring_buffer_del_replica_mask(struct ring_buffer *rb, unsigned replicas);

/**
 * Insert an element to the ring_buffer.
 * Must be only called by the writer thread.
 * @returns a replica_id mask of offending replicas.
 */
unsigned ring_buffer_insert(struct ring_buffer * restrict rb, ring_buffer_eid_t id, void * restrict p);

/**
 * Get an element from the ring_buffer.
 * Must be only called by a replica thread.
 * ring_buffer_mark_replicated() must be called for each element e accessed with this
 * function to allow the ring_buffer to advance.
 * @returns 1 if an element was read; 0 if we were dropped out from the ring buffer.
 */
int ring_buffer_get_next(struct ring_buffer *rb, struct ring_buffer_reader_state *state, struct ring_buffer_element **e);

/**
 * Mark the element e in a ring buffer as replicated.
 * Marking an element as replicated allows ring_buffer_insert() to write over the element.
 *
 */
void ring_buffer_mark_replicated(struct ring_buffer_reader_state *state, struct ring_buffer_element *e);
