/*
 * Copyright (c) 2020-2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "mempool.h"

#define MOD_AL(x, y) ((x) & ((y) - 1)) /* x % bytes */
#define PAD(size, al) MOD_AL(((al) - MOD_AL((size), (al))), (al))
#define ALIGNED_SIZE(size, al) ((size) + PAD((size), (al)))

/**
 * Slab descriptor for a mempool.
 * This struct is used to temporarily hold the slab size information shared by
 * all slabs in a pool.
 */
struct slab_info {
    size_t total_bytes;
    size_t chunk_size;
    size_t obj_size;
    size_t nr_objects;
};

static char *get_first_chunk(struct mempool_slab * restrict slab, const struct slab_info * restrict info) {
    return ((char *)slab) + sizeof(struct mempool_slab) + info->chunk_size - info->obj_size;
}

/**
 * Calculate slab_info for mempool.
 */
static struct slab_info slab_info(const struct mempool * restrict mempool) {
    const size_t slab_size = mempool->slab_size_kb * 1024;
    const size_t chunk_size = ALIGNED_SIZE(
            sizeof(struct mempool_object) +
            PAD(sizeof(struct mempool_object), mempool->obj_align) +
            mempool->obj_size,
            alignof(struct mempool_object));
    const size_t nr_total = (slab_size - sizeof(struct mempool_slab)) / chunk_size;

    assert(nr_total > 0);

    return (struct slab_info){
        .total_bytes = slab_size,
        .chunk_size = chunk_size,
        .obj_size = mempool->obj_size,
        .nr_objects = nr_total,
    };
}

void mempool_init(struct mempool *mempool, size_t slab_size, size_t obj_size, size_t obj_align) {
    assert(slab_size - sizeof(struct mempool_slab) > obj_size &&
           slab_size / 1024 > 0 &&
           slab_size / 1024 < UINT16_MAX &&
           ALIGNED_SIZE(slab_size, 512) == slab_size);
    assert(obj_size < UINT16_MAX);
    assert(obj_align < UINT16_MAX);

    mempool->slab_size_kb = slab_size / 1024;
    mempool->obj_size = obj_size;
    mempool->obj_align = obj_align;
    SLIST_INIT(&mempool->slabs);
    LIST_INIT(&mempool->free_objects);
}

/**
 * Free slab that was allocated in mempool
 */
static void mempool_free_slab(const struct mempool *mempool, struct mempool_slab *slab) {
    (void)munmap(slab, mempool->slab_size_kb * 1024);
}

void mempool_destroy(struct mempool *mempool) {
    /*
     * We don't keep track of the slab pointers because we assume the user to
     * know the slabs and return every single one of them before destroying the
     * mempool.
     */
    while (!SLIST_EMPTY(&mempool->slabs)) {
        struct mempool_slab *slab;

        slab = SLIST_FIRST(&mempool->slabs);
        SLIST_REMOVE_HEAD(&mempool->slabs, next_slab);
        mempool_free_slab(mempool, slab);
    }

    memset(mempool, 0, sizeof(*mempool));
}

void mempool_gc(struct mempool *mempool) {
    struct slab_info info = slab_info(mempool);
    struct mempool_slab *slab;
    struct mempool_slab *slab_temp;

    /*
     * Go through all the slabs and find the ones that have no object
     * allocations.
     */
    SLIST_FOREACH_SAFE(slab, &mempool->slabs, next_slab, slab_temp) {
        if (slab->nr_free == info.nr_objects) {

            SLIST_REMOVE(&mempool->slabs, slab, mempool_slab, next_slab);

            /*
             * Remove all the objects of this slab from the free list.
             */
            char * chunk = get_first_chunk(slab, &info);

            for (size_t i = 0; i < info.nr_objects; i++) {
                struct mempool_object *o;

                o = (struct mempool_object *)chunk;
                LIST_REMOVE(o, next_free);
                chunk += info.chunk_size;
            }

            mempool_free_slab(mempool, slab);
        }
    }
}

/**
 * Allocate a new slab using mmap().
 */
static int mempool_new_slab(struct mempool *mempool) {
    struct mempool_slab *slab;

    slab = mmap(0, mempool->slab_size_kb * 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (slab == MAP_FAILED) {
        return 1;
    }

    const struct slab_info info = slab_info(mempool);

    slab->nr_free = info.nr_objects;

    /*
     * Add all new objects to the list of free objects in the pool.
     */
    char * chunk = get_first_chunk(slab, &info);
    for (size_t i = 0; i < info.nr_objects; i++) {
        struct mempool_object *o = (struct mempool_object *)chunk;

        o->slab = slab;
        LIST_INSERT_HEAD(&mempool->free_objects, o, next_free);

        chunk += info.chunk_size;
    }

    SLIST_INSERT_HEAD(&mempool->slabs, slab, next_slab);

    return 0;
}

void *mempool_get(struct mempool *mempool) {
    struct mempool_object *next;

    if (LIST_EMPTY(&mempool->free_objects)) {
        int err;

        err = mempool_new_slab(mempool);
        if (err) {
            return NULL;
        }
    }

    next = LIST_FIRST(&mempool->free_objects);
    LIST_REMOVE(next, next_free);
    next->slab->nr_free--;

    return ((char *)next) + sizeof(struct mempool_object);
}

void mempool_return(struct mempool *mempool, void *p) {
    struct mempool_object *o = (struct mempool_object *)(((char *)p) - sizeof(struct mempool_object));

    LIST_INSERT_HEAD(&mempool->free_objects, o, next_free);
    o->slab->nr_free++;

    /*
     * Not that we never free slabs here. Slabs are only removed when the user
     * explicitly calls mempool_gc().
     *
     * Some fragmentation may occur in the allocator as we are not preferring
     * partially full slabs when getting new objects. Therefore, we may end up
     * with a lot of partially full slabs while the optimal utilization would
     * have mostly full slabs.
     */
}
