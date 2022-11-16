#include <punit.h>
#include <stdio.h>
#include "cdefs.h"
#include "queue.h"
#include "util/finalizer.h"

static int fin;

static void setup(void)
{
    fin = 0;
}

static void teardown(void)
{
}

static void dispose(void *p)
{
    unsigned i = (unsigned)p;

    fin |= i;
}

static char * test_finalizer(void)
{
    struct finalizer f;

    finalizer_init(&f);
    finalizer_add(&f, (void *)0x1,dispose);
    finalizer_add(&f, (void *)0x2, dispose);
    finalizer_run(&f);

    pu_assert_equal("", fin, 0x3);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_finalizer, PU_RUN);
}
