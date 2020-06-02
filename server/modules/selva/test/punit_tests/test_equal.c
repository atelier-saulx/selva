/* file test_equal.c */

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
    int value = 4;

    pu_assert_equal("Values are equal", value, 4);
    return 0;
}

static char * test_fail(void)
{
    int value = 4;

    pu_assert_equal("Values are equal", value, 5);
    return 0;
}

void all_tests(void)
{
    pu_run_test(test_ok);
    pu_run_test(test_fail);
}
