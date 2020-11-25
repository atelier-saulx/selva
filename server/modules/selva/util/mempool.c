#include <stddef.h>
#include <sys/mman.h>
#include "redismodule.h"
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
} __attribute__((aligned(8)));

/**
 * A structure describing a slab in the pool allocator.
 */
struct mempool_slab {
    size_t nr_free;
    SLIST_ENTRY(mempool_slab) next_slab;
};

/**
 * A structure describing a memory pool.
 */
struct mempool {
    size_t slab_size;
    size_t obj_size;
    SLIST_HEAD(mempool_slab_list, mempool_slab) slabs;
    LIST_HEAD(mempool_free_object_list, mempool_object) free_objects;
};

struct slab_info {
    size_t total_bytes;
    size_t chunk_size;
    size_t obj_size;
    size_t nr_objects;
};

static struct slab_info slab_info(const struct mempool * restrict mempool) {
    const size_t chunk_size = sizeof(struct mempool_object) + mempool->obj_size;
    const size_t nr_total = (mempool->slab_size - sizeof(struct mempool_slab)) / chunk_size;

    return (struct slab_info){
        .total_bytes = mempool->slab_size,
        .chunk_size = chunk_size,
        .obj_size = mempool->obj_size,
        .nr_objects = nr_total,
    };
}

struct mempool *mempool_new(size_t slab_size, size_t obj_size) {
    struct mempool *mempool;

    mempool = RedisModule_Alloc(sizeof(struct mempool));
    if (!mempool) {
        return NULL;
    }

    mempool->slab_size = slab_size;
    mempool->obj_size = obj_size;
    SLIST_INIT(&mempool->slabs);
    LIST_INIT(&mempool->free_objects);

    return mempool;
}

static void mempool_free_slab(struct mempool *mempool, struct mempool_slab *slab) {
    (void)munmap(slab, mempool->slab_size);
}

void mempool_destroy(struct mempool *mempool) {
    if (!mempool) {
        return;
    }

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

    RedisModule_Free(mempool);
}

void mempool_gc(struct mempool *mempool) {
    struct slab_info info = slab_info(mempool);
    struct mempool_slab *slab;
    struct mempool_slab *slab_temp;

	SLIST_FOREACH_SAFE(slab, &mempool->slabs, next_slab, slab_temp) {
        if (slab->nr_free == info.nr_objects) {

            SLIST_REMOVE(&mempool->slabs, slab, mempool_slab, next_slab);

            /*
             * Remove all the objects of this slab from the free list.
             */
            char * chunk = ((char *)slab) + sizeof(struct mempool_slab);

            for (size_t i = 0; i < info.nr_objects; i++) {
                struct mempool_object *o;

                o = (struct mempool_object *)chunk;
                LIST_REMOVE(o, next_free);
                chunk += sizeof(struct mempool_object) + info.obj_size;
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

    slab = mmap(0, mempool->slab_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (slab == MAP_FAILED) {
        return 1;
    }

    const struct slab_info info = slab_info(mempool);

    slab->nr_free = info.nr_objects;

    /*
     * Add all new objects to the list of free objects in the pool.
     */
    char * chunk = ((char *)slab) + sizeof(struct mempool_slab);
    for (size_t i = 0; i < info.nr_objects; i++) {
        struct mempool_object *o = (struct mempool_object *)chunk;

        o->slab = slab;
        LIST_INSERT_HEAD(&mempool->free_objects, o, next_free);

        chunk += sizeof(struct mempool_object) + info.obj_size;
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
