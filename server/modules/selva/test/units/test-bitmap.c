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
    bitmap_set(bmap, 130);
    bitmap_set(bmap, 319);
    bitmap_set(bmap, 0);
    bitmap_set(bmap, 126);
    bitmap_set(bmap, 127);
    bitmap_set(bmap, 128);
    bitmap_set(bmap, 129);
    bitmap_set(bmap, 60);
    pu_assert_equal("fails to set over lim", bitmap_set(bmap, 320), -1);

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
    pu_def_test(test_bmap, PU_RUN);
}
