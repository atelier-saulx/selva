/* file test_doubles.c */

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
    double value = 4.0f;

    pu_assert_double_equal("Values are approximately equal", value, 4.2f, 0.3f);
    return 0;
}

static char * test_fail(void)
{
    double value = 3.0f;

    pu_assert_double_equal("Values are approximately equal", value, 5.0f, 0.5f);
    return 0;
}

void all_tests(void)
{
    pu_run_test(test_ok);
    pu_run_test(test_fail);
}
