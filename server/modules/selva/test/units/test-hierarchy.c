#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "hierarchy.h"
#include "cdefs.h"

static void setup(void)
{
}

static void teardown(void)
{
}

static char * test_init_works(void)
{
    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_init_works, PU_RUN);
}
