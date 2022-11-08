#include <punit.h>
#include <stdalign.h>
#include <stdint.h>
#include "util/mempool.h"

static void setup(void)
{
}

static void teardown(void)
{
}

static char * test_simple_allocs(void)
{
    const size_t slab_size = 4194304;
    const size_t obj_size = 256;
    struct mempool pool;

    mempool_init(&pool, slab_size, obj_size, alignof(size_t));

    char *p1 = mempool_get(&pool);
    snprintf(p1, obj_size, "Hello world\n");

    char *p2 = mempool_get(&pool);
    snprintf(p2, obj_size, "Hallo world\n");

    pu_assert("pointers differ", p1 != p2);
    pu_assert_str_equal("p1 has the correct string", p1, "Hello world\n");
    pu_assert_str_equal("p2 has the correct string", p2, "Hallo world\n");

    mempool_return(&pool, p1);
    mempool_return(&pool, p2);
    mempool_destroy(&pool);

    return NULL;
}

static char * test_object_reuse(void)
{
    const size_t slab_size = 4194304;
    const size_t obj_size = 256;
    struct mempool pool;

    mempool_init(&pool, slab_size, obj_size, alignof(size_t));

    char *p1 = mempool_get(&pool);
    (void)mempool_get(&pool);

    mempool_return(&pool, p1);

    char *p3 = mempool_get(&pool);

    pu_assert_ptr_equal("The object is reused", p3, p1);

    mempool_destroy(&pool);

    return NULL;
}

static char * test_gc(void)
{
    const size_t slab_size = 4194304;
    const size_t obj_size = 256;
    struct mempool pool;

    mempool_init(&pool, slab_size, obj_size, alignof(size_t));

    char *p1 = mempool_get(&pool);
    mempool_return(&pool, p1);
    mempool_gc(&pool);

    struct mempool pool2;
    mempool_init(&pool2, slab_size, obj_size, alignof(size_t));
    pu_assert("The second pool works", mempool_get(&pool2));

    char *p2 = mempool_get(&pool);

    pu_assert("A new slab was allocated", p1 != p2);

    mempool_destroy(&pool);
    mempool_destroy(&pool2);

    return NULL;
}

static char * test_allocs(void)
{
    const size_t slab_size = 1024;
    const size_t obj_size = 100;
    struct mempool pool;
    char *p;

    mempool_init(&pool, slab_size, obj_size, 1);

    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);
    p = mempool_get(&pool);
    pu_assert("got obj", p);

    mempool_destroy(&pool);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_simple_allocs, PU_RUN);
    pu_def_test(test_object_reuse, PU_RUN);
    pu_def_test(test_gc, PU_RUN);
    pu_def_test(test_allocs, PU_RUN);
}
