/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

typedef int64_t ring_buffer_eid_t; /*!< Element id type. */

/*
 * We use atomic_uint to access reader masks. Otherwise reader ids and masks
 * are passed using the unsigned type.
 * We assume to have at least 32 reader ids available.
 */
static_assert(sizeof(atomic_uint) >= sizeof(unsigned));
static_assert(sizeof(unsigned) >= sizeof(uint32_t));

/**
 * An element of a ring_buffer.
 */
struct ring_buffer_element {
    ring_buffer_eid_t id; /*!< Element id used for lookup in ring_buffer_init_state(). */
    /**
     * Element not yet read/processed.
     * A bitmask of readers that have not yet received the data pointer by this
     * element.
     */
    atomic_uint not_read;
    void *data; /*!< A pointer to the data. */
    size_t data_size;
};

struct ring_buffer {
    size_t tail;
    size_t len;
    struct ring_buffer_element *buf;
    /**
     * A pointer to a function to free the data of a ring_buffer_element.
     * This function will be called when an element is replaced in the ring
     * buffer, that happens only if the element has been marked as read
     * for every replice, i.e. not_read = 0.
     */
    void (*free_element_data)(void *p, ring_buffer_eid_t eid);
    /**
     * A bitmask of all active readers.
     * Updated with ring_buffer_add_reader() and ring_buffer_del_reader().
     */
    atomic_uint readers_mask;
    pthread_mutex_t lock;
    pthread_cond_t cond; /*!< Used to wait for new insertions. */
};

/**
 * A state for each reader thread i.e. reader of the ring_buffer.
 */
struct ring_buffer_reader_state {
    size_t index; /*!< Index of the last read element. */
    unsigned reader_id; /*!< The id of this reader. */
};

/**
 * Initialize a ring_buffer.
 * @param rb is a pointer to an unintialized ring_buffer.
 * @param free_element_data is a pointer to a function that can free a pointer
 * p given to the ring_buffer_insert() function.
 */
void ring_buffer_init(struct ring_buffer* rb, struct ring_buffer_element *buf, size_t nelem, void (*free_element_data)(void *p, ring_buffer_eid_t eid));

/**
 * Initialize a ring_buffer_state structure.
 * At least one element must be set in the ring buffer rb before a new reader
 * state can be initialized. Ideally this function is called by the
 * reader thread and ring_buffer_add_reader() is called by the writer
 * thread before this function is called or the thread is even created.
 * @param state is a pointer to an uninitialized ring_buffer_reader_state structure.
 * @param rb is a pointer to an initialized ring_buffer.
 * @param id is an id of an element known to exist in rb that doesn't need to be read by the reader i.e. initial state.
 * @param reader_id is the unique id assigned to this reader.
 */
int ring_buffer_init_state(struct ring_buffer_reader_state* state, struct ring_buffer* rb, ring_buffer_eid_t id, unsigned reader_id);

/**
 * Add a reader to the ring_buffer.
 * Must be called by the writer before the reader thread is started.
 * @param rb is a pointer to an initialized ring buffer.
 * @param reader_id is an unique id of a reader thread that will join as a reader.
 */
void ring_buffer_add_reader(struct ring_buffer *rb, unsigned reader_id);

/**
 * Delete all readers in the mask from the ring_buffer.
 * @param rb is a pointer to the ring buffer.
 * @param readers is a bitmask of readers to be removed.
 */
void ring_buffer_del_readers_mask(struct ring_buffer *rb, unsigned readers);

/**
 * Delete a reader from the ring_buffer.
 * Must be called by the writer reader. After this function the next
 * ring_buffer_get_next() call by the reader will fail.
 * @param rb is a pointer to the ring buffer.
 * @param reader_id is the unique id of the reader to be removed.
 */
void ring_buffer_del_reader(struct ring_buffer *rb, unsigned reader_id);

/**
 * Release all elements assigned for this reader.
 * Can be called by the the writer thread. If called by the
 * writer then the next ring_buffer_get_next() call by the reader willfail.
 */
void ring_buffer_reader_exit(struct ring_buffer *rb, struct ring_buffer_reader_state *state);

/**
 * Insert an element to the ring_buffer.
 * Must be only called by the writer thread.
 * @param rb is a pointer to the ring buffer.
 * @param id is an unique identifier assigned to the new element.
 * @param p is a pointer to the data associated with id.
 * @param size is the size of of p in bytes.
 * @returns a reader_id mask of offending readers.
 */
unsigned ring_buffer_insert(struct ring_buffer * restrict rb, ring_buffer_eid_t id, void * restrict p, size_t size);

/**
 * Get an element from the ring_buffer.
 * Must be only called by a reader thread.
 * ring_buffer_release() must be called for each element e accessed with this
 * function to allow the ring_buffer to advance.
 * @param rb is a pointer to the ring buffer.
 * @param state is a pointer to the reader state of the reader thread.
 * @param[out] e returns a pointer to the element read from the ring buffer.
 * @returns 1 if an element was read; 0 if we were dropped out from the ring buffer.
 */
int ring_buffer_get_next(struct ring_buffer *rb, struct ring_buffer_reader_state *state, struct ring_buffer_element **e);

/**
 * Mark the element e in a ring buffer as read.
 * Marking an element as read allows ring_buffer_insert() to write over the element.
 * @param state is a pointer to the reader state of the reader thread.
 * @param e is a pointer to the ring buffer element returned by ring_buffer_get_next().
 */
void ring_buffer_release(struct ring_buffer_reader_state *state, struct ring_buffer_element *e);
