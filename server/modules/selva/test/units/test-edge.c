#include <punit.h>
#include "edge.h"

static void setup(void)
{
}

static void teardown(void)
{
}

static char * test_get_constraint(void)
{
    struct EdgeFieldConstraint *constraint;

    constraint = Edge_GetConstraint(0);
    pu_assert("got the default constraint", Edge_GetConstraint(0));
    pu_assert("got null", !Edge_GetConstraint((unsigned)(-1)));

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_get_constraint, PU_RUN);
}
