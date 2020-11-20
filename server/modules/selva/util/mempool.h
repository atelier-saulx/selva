#pragma once
#ifndef _UTIL_MEMPOOL_H_
#define _UTIL_MEMPOOL_H_

#include "cdefs.h"

struct mempool;

/**
 * Allocate a new mempool slab allocator.
 */
struct mempool *mempool_new(size_t slab_size, size_t obj_size);

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
