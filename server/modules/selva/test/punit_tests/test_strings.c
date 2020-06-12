/* file test_strings.c */

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
    const char *str = "left string";

    pu_assert_str_equal("Strings are equal", str, "left string");
    return 0;
}

static char * test_fail(void)
{
    const char *str = "left string";

    pu_assert_str_equal("Strings are equal", str, "right string");
    return 0;
}

void all_tests(void)
{
    pu_run_test(test_ok);
    pu_run_test(test_fail);
}
