/* file test_example.c */

#include <stdio.h>
#include "punit.h"

int foo;
int bar;

static void setup()
{
    foo = 7;
    bar = 4;
}

static void teardown()
{
}

static char * test_foo()
{
    pu_test_description("This test case will just demostrate usage of the most basic assert function.");

    pu_assert("error, foo != 7", foo == 7);
    return 0;
}

static char * test_derp()
{
    pu_assert("error, bar != 5", bar == 4);
    return 0;
}

void all_tests()
{
    pu_mod_description("This is an example of a test module.");
    pu_def_test(test_foo, PU_RUN);
    pu_def_test(test_derp, PU_SKIP);
}
