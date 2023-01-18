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

/**
 * An element of a ring_buffer.
 */
struct ring_buffer_element {
    ring_buffer_eid_t id; /*!< Element id used for lookup in ring_buffer_init_state(). */
    /**
     * Element not yet replicated.
     * A bitmask of replicas that have not yet received the data pointer by this
     * element.
     */
    atomic_uint not_replicated;
    void *data; /*!< A pointer to the data. */
};

struct ring_buffer {
    struct ring_buffer_element buffer[RING_BUFFER_SIZE];
    size_t tail;
    size_t len;
    /**
     * A pointer to a function to free the data of a ring_buffer_element.
     * This function will be called when an element is replaced in the ring
     * buffer, that happens only if the element has been marked as replicated
     * for every replice, i.e. not_replicated = 0.
     */
    void (*free_element_data)(void *p, ring_buffer_eid_t eid);
    /**
     * A bitmask of all active replicas.
     * Updated with ring_buffer_add_replica() and ring_buffer_del_replica().
     */
    atomic_uint replicas_mask;
    pthread_mutex_t lock;
    pthread_cond_t cond; /*!< Used to wait for new insertions. */
};

/**
 * A state for each replica thread i.e. reader of the ring_buffer.
 */
struct ring_buffer_reader_state {
    size_t index; /*!< Index of the last read element. */
    unsigned replica; /*!< The id of this replica. */
};

/**
 * Initialize a ring_buffer.
 * @param rb is a pointer to an unintialized ring_buffer.
 * @param free_element_data is a pointer to a function that can free a pointer
 * p given to the ring_buffer_insert() function.
 */
void ring_buffer_init(struct ring_buffer* rb, void (*free_element_data)(void *p, ring_buffer_eid_t eid));

/**
 * Initialize a ring_buffer_state structure.
 * At least one element must be set in the ring buffer rb before a new reader
 * state can be initialized. Ideally this function is called by the
 * replica/reader thread and ring_buffer_add_replica() is called by the writer
 * thread before this function is called or the thread is even created.
 * @param state is a pointer to an uninitialized ring_buffer_reader_state structure.
 * @param rb is a pointer to an initialized ring_buffer.
 * @param id is an id known to exist in rb that doesn't need to be replicated.
 * @param replica_id is the unique id assigned to this replica.
 */
int ring_buffer_init_state(struct ring_buffer_reader_state* state, struct ring_buffer* rb, ring_buffer_eid_t id, unsigned replica_id);

/**
 * Add a replica to the ring_buffer.
 * Must be called by the writer before the replica thread is started.
 * @param rb is a pointer to an initialized ring buffer.
 * @param replica_id is an unique id of a replica thread that will join as a reader.
 */
void ring_buffer_add_replica(struct ring_buffer *rb, unsigned replica_id);

/**
 * Delete a replica from the ring_buffer.
 * Can be called by the replication thead or the writer. If called by the
 * writer then the next ring_buffer_get_next() call by the reader willfail.
 * @param rb is a pointer to the ring buffer.
 * @param replica_id is the unique id of the replica to be removed.
 */
void ring_buffer_del_replica(struct ring_buffer *rb, unsigned replica_id);

/**
 * Calls ring_buffer_del_replica() for each replica in the replicas bitmask.
 * @param rb is a pointer to the ring buffer.
 * @param replicas is a bitmask of replicas to be removed.
 */
void ring_buffer_del_replica_mask(struct ring_buffer *rb, unsigned replicas);

/**
 * Insert an element to the ring_buffer.
 * Must be only called by the writer thread.
 * @param rb is a pointer to the ring buffer.
 * @param id is an unique identifier assigned to the new element.
 * @param p is a pointer to the data associated with id.
 * @returns a replica_id mask of offending replicas.
 */
unsigned ring_buffer_insert(struct ring_buffer * restrict rb, ring_buffer_eid_t id, void * restrict p);

/**
 * Get an element from the ring_buffer.
 * Must be only called by a replica thread.
 * ring_buffer_mark_replicated() must be called for each element e accessed with this
 * function to allow the ring_buffer to advance.
 * @param rb is a pointer to the ring buffer.
 * @param state is a pointer to the reader state of the replica thread.
 * @param[out] e returns a pointer to the element read from the ring buffer.
 * @returns 1 if an element was read; 0 if we were dropped out from the ring buffer.
 */
int ring_buffer_get_next(struct ring_buffer *rb, struct ring_buffer_reader_state *state, struct ring_buffer_element **e);

/**
 * Mark the element e in a ring buffer as replicated.
 * Marking an element as replicated allows ring_buffer_insert() to write over the element.
 * @param state is a pointer to the reader state of the replica thread.
 * @param e is a pointer to the ring buffer element returned by ring_buffer_get_next().
 */
void ring_buffer_mark_replicated(struct ring_buffer_reader_state *state, struct ring_buffer_element *e);
