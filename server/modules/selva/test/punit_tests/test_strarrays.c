/* file test_arrays.c */

#include <stdio.h>
#include "punit.h"

static void setup(void)
{
}

static void teardown(void)
{
}

static char * test_ok(void)
{
    char * arr1[] = {"one", "two", "three"};
    char * arr2[] = {"one", "two", "three"};

    pu_assert_str_array_equal("Arrays are equal", arr1, arr2, sizeof(arr1)/sizeof(*arr1));
    return 0;
}

static char * test_fail(void)
{
    char * arr1[] = {"one", "two", "three"};
    char * arr2[] = {"one", "three", "four"};

    pu_assert_str_array_equal("Arrays are equal", arr1, arr2, sizeof(arr1)/sizeof(*arr1));
    return 0;
}

void all_tests(void)
{
    pu_run_test(test_ok);
    pu_run_test(test_fail);
}
