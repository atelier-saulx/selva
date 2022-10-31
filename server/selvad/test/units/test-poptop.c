#include <punit.h>
#include <tgmath.h>
#include "cdefs.h"
#include "util/poptop.h"

struct my_data {
    int v;
};

struct poptop l;

static void setup(void)
{
    memset(&l, 0, sizeof(l));
}

static void teardown(void)
{
    poptop_deinit(&l);
}

static char * test_add_rm(void)
{
    const size_t max_size = 10;
    const float initial_cut = 0.0;
    float score = 5.0;
    struct my_data d = {
        .v = 42,
    };
    int err;
    struct poptop_list_el *el;

    err = poptop_init(&l, max_size, initial_cut);
    pu_assert_equal("initialized", err, 0);

    poptop_maybe_add(&l, score, &d);
    score = 6.0;
    poptop_maybe_add(&l, score, &d);

    POPTOP_FOREACH(el, &l) {
        struct my_data * pd = el->p;

        if (!pd) {
            /* el is only valid if pd is non-NULL. */
            continue;
        }

        pu_assert_equal("found right elem", pd->v, 42);
        pu_assert_equal("score was updated", el->score, 6.0);
    }

    poptop_remove(&l, &d);

    POPTOP_FOREACH(el, &l) {
        struct my_data * pd = el->p;

        pu_assert_null("all null", pd);
    }

    return NULL;
}

static char * test_add_too_many(void)
{
    int err;
    struct my_data d[] = {
        {
            .v = 10,
        },
        {
            .v = 11,
        },
        {
            .v = 12,
        },
        {
            .v = 13,
        },
        {
            .v = 14,
        },
        {
            .v = 15,
        },
        {
            .v = 16,
        },
        {
            .v = 17,
        },
        {
            .v = 18,
        },
        {
            .v = 19,
        },
    };
    struct poptop_list_el *el;

    err = poptop_init(&l, 5, 0.0f);
    pu_assert_equal("initialized", err, 0);

    for (size_t i = 0; i < num_elem(d); i++) {
        poptop_maybe_add(&l, (float)i, d + i);
    }

    POPTOP_FOREACH(el, &l) {
        struct my_data * pd = el->p;

        if (!pd) {
            continue;
        }

        pu_assert("expected v range", pd->v < 15);
    }

    return NULL;
}

static char * test_foreach(void)
{
    int err;
    struct my_data d[] = {
        {
            .v = 10,
        },
        {
            .v = 11,
        },
        {
            .v = 12,
        },
        {
            .v = 13,
        },
        {
            .v = 14,
        },
        {
            .v = 15,
        },
        {
            .v = 16,
        },
        {
            .v = 17,
        },
        {
            .v = 18,
        },
        {
            .v = 19,
        },
    };
    struct poptop_list_el *el;

    err = poptop_init(&l, 10, 5.0f);
    pu_assert_equal("initialized", err, 0);

    for (size_t i = 0; i < num_elem(d); i++) {
        poptop_maybe_add(&l, (float)i, d + i);
    }

    POPTOP_FOREACH(el, &l) {
        struct my_data * pd = el->p;

        if (!pd) {
            continue;
        }

        switch (pd->v) {
        case 15 ... 19:
            pu_assert_equal("expected score", pd->v - 10, (int)el->score);
            break; /* NOP */
        default:
            pu_assert_fail("this elem shouldn't be in the list");
        }
    }

    poptop_remove(&l, d + 7);

    POPTOP_FOREACH(el, &l) {
        struct my_data * pd = el->p;

        if (!pd) {
            continue;
        }

        switch (pd->v) {
        case 17:
            pu_assert_fail("17 shouldn't be in the list anymore");
        case 10 ... 16:
        case 18 ... 19:
            pu_assert_equal("expected score", pd->v - 10, (int)el->score);
            break; /* NOP */
        default:
            pu_assert_fail("this elem shouldn't be in the list");
        }
    }

    return NULL;
}

static char * test_maintenance(void)
{
    int err;
    struct my_data d[] = {
        {
            .v = 10,
        },
        {
            .v = 11,
        },
        {
            .v = 12,
        },
        {
            .v = 13,
        },
        {
            .v = 14,
        },
        {
            .v = 15,
        },
        {
            .v = 16,
        },
        {
            .v = 17,
        },
        {
            .v = 18,
        },
        {
            .v = 19,
        },
    };
    struct my_data *dp;
    int drop_count = 0;

    err = poptop_init(&l, 10, 0.0f);
    pu_assert_equal("initialized", err, 0);

    for (size_t i = 0; i < num_elem(d); i++) {
        poptop_maybe_add(&l, (float)i, d + i);
    }

    poptop_maintenance(&l);

    pu_assert_equal("new cut_limit is set", round(l.cut_limit), 5.0f);

    while ((dp = poptop_maintenance_drop(&l))) {
        pu_assert("expected drop", dp->v < 15);
        drop_count++;
    }

    pu_assert_equal("dropped", drop_count, 5);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_add_rm, PU_RUN);
    pu_def_test(test_add_too_many, PU_RUN);
    pu_def_test(test_foreach, PU_RUN);
    pu_def_test(test_maintenance, PU_RUN);
}
