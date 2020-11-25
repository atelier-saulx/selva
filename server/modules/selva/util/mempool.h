#pragma once
#ifndef _UTIL_MEMPOOL_H_
#define _UTIL_MEMPOOL_H_

#include "cdefs.h"
#include "queue.h"

/**
 * A structure describing the object allocation unit.
 */
struct mempool_object {
    struct mempool_slab *slab; /*!< A pointer back the slab. */
    /**
     * A list entry pointing to the next free object if this object is in the
     * free list.
     */
    LIST_ENTRY(mempool_object) next_free;
} __attribute__((aligned(sizeof(size_t))));

/**
 * A structure describing a slab in the pool allocator.
 */
struct mempool_slab {
    size_t nr_free;
    SLIST_ENTRY(mempool_slab) next_slab;
} __attribute__((aligned((sizeof(size_t)))));

/**
 * A structure describing a memory pool.
 */
struct mempool {
    size_t slab_size;
    size_t obj_size;
    SLIST_HEAD(mempool_slab_list, mempool_slab) slabs;
    LIST_HEAD(mempool_free_object_list, mempool_object) free_objects;
};

/**
 * Initialize a new mempool slab allocator.
 */
void mempool_init(struct mempool *mempool, size_t slab_size, size_t obj_size);

/**
 * Destroy a mempool and free all memory.
 * Note that this function doesn't check whether all objects have been
 * returned.
 */
void mempool_destroy(struct mempool *mempool);

/**
 * Free all unused slabs.
 */
void mempool_gc(struct mempool *mempool);

/**
 * Get a new object from the pool.
 */
void *mempool_get(struct mempool *mempool);

/**
 * Return an object back to the pool.
 */
void mempool_return(struct mempool *mempool, void *p);

#endif /* _UTIL_MEMPOOl_H_ */
