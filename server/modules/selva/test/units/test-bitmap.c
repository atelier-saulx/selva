#include <stdlib.h>
#include <punit.h>
#include "bitmap.h"

static struct bitmap *bmap;

static void setup(void)
{
}

static void teardown(void)
{
    free(bmap);
}

static char * test_bmap_single(void)
{
    const size_t bsize = BITMAP_ALLOC_SIZE(1);
    pu_assert("size is greater than the struct size", bsize > sizeof(struct bitmap));

    bmap = malloc(bsize);
    pu_assert("allocated", bmap);
    bmap->nbits = 1;

    bitmap_erase(bmap);
    pu_assert_equal("bmap is empty", bitmap_popcount(bmap), 0);

    int res = bitmap_set(bmap, 0);
    pu_assert_equal("no error", res, 0);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 0), 1);

    return NULL;
}

static char * test_bmap_clear(void)
{
    const size_t bsize = BITMAP_ALLOC_SIZE(320);

    pu_assert_equal("required size is calculated correctly", bsize, sizeof(struct bitmap) + 3 * 16);

    bmap = malloc(bsize);
    pu_assert("allocated", bmap);
    bmap->nbits = 32;

    bitmap_erase(bmap);
    pu_assert_equal("bmap is empty", bitmap_popcount(bmap), 0);

    bitmap_set(bmap, 0);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 0), 1);
    bitmap_clear(bmap, 0);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 0);

    return NULL;
}

static char * test_bmap_erase(void)
{
    const size_t bsize = BITMAP_ALLOC_SIZE(320);

    pu_assert_equal("required size is calculated correctly", bsize, sizeof(struct bitmap) + 3 * 16);

    bmap = malloc(bsize);
    pu_assert("allocated", bmap);
    bmap->nbits = 32;

    bitmap_erase(bmap);
    pu_assert_equal("bmap is empty", bitmap_popcount(bmap), 0);

    bitmap_set(bmap, 0);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 0), 1);
    bitmap_erase(bmap);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 0);
    pu_assert_equal("nbits is preserved", bmap->nbits, 32);

    return NULL;
}

static char * test_bmap(void)
{
    const size_t bsize = BITMAP_ALLOC_SIZE(320);

    pu_assert_equal("required size is calculated correctly", bsize, sizeof(struct bitmap) + 3 * 16);

    bmap = malloc(bsize);
    pu_assert("allocated", bmap);
    bmap->nbits = 320;

    bitmap_erase(bmap);
    pu_assert_equal("bmap is empty", bitmap_popcount(bmap), 0);

    bitmap_set(bmap, 1);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 1);
    bitmap_set(bmap, 130);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 2);
    bitmap_set(bmap, 319);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 3);
    bitmap_set(bmap, 0);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 4);
    bitmap_set(bmap, 126);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 5);
    bitmap_set(bmap, 127);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 6);
    bitmap_set(bmap, 128);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 7);
    bitmap_set(bmap, 129);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 8);
    bitmap_set(bmap, 60);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 9);
    pu_assert_equal("fails to set over lim", bitmap_set(bmap, 320), -1);
    pu_assert_equal("popcount", bitmap_popcount(bmap), 9);

    pu_assert_equal("bit is set", bitmap_get(bmap, 1), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 130), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 319), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 0), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 126), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 128), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 129), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 60), 1);
    pu_assert_equal("bit is set", bitmap_get(bmap, 2), 0);
    pu_assert_equal("bit is set", bitmap_get(bmap, 131), 0);
    pu_assert_equal("bit is set", bitmap_get(bmap, 132), 0);
    pu_assert_equal("bit is set", bitmap_get(bmap, 133), 0);
    pu_assert_equal("bit is set", bitmap_get(bmap, 318), 0);
    pu_assert_equal("bit is set", bitmap_get(bmap, 320), -1);

    pu_assert_equal("popcount ok", bitmap_popcount(bmap), 9);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_bmap_single, PU_RUN);
    pu_def_test(test_bmap_clear, PU_RUN);
    pu_def_test(test_bmap_erase, PU_RUN);
    pu_def_test(test_bmap, PU_RUN);
}
