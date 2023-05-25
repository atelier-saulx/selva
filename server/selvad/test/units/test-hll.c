/*
 * Copyright (c) 2023 SAULX
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <punit.h>
#include "util/hll.h"

static hll_t *hll;

static void setup(void)
{
    hll = hll_create();
}

static void teardown(void)
{
    hll_destroy(hll);
}

#define CALC_ERR(ve, p) \
    ((long long)((double)(ve) * (double)(p)))
#define WITHIN_ERR(act, ve, p) \
    ((act) >= ((ve) - CALC_ERR((ve), (p))) && ((act) <= ((ve) + CALC_ERR((ve), (p)))))

static char * test_hll_sparse(void)
{
#define N 100
    long long n = 0, act;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < 2; j++) {
            char buf[80];

            snprintf(buf, sizeof(buf), "%d", i + j);

            hll_add(&hll, buf, strlen(buf));
            n++;
        }
    }

    act = hll_count(hll);
    pu_assert("count correct", WITHIN_ERR(act, N, 0.02));

    hll_t *dense = hll_sparse_to_dense(hll);
    pu_assert_ptr_not_equal("converted only now", dense, hll);
    hll = dense;

    return NULL;
#undef N
}

static char * test_hll_huge(void)
{
#define N 100000
    long long n = 0, act;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < 2; j++) {
            char buf[80];

            snprintf(buf, sizeof(buf), "%d", i + j);

            hll_add(&hll, buf, strlen(buf));
            n++;
        }
    }

    act = hll_count(hll);
    pu_assert("count correct", WITHIN_ERR(act, N, 0.02));

    return NULL;
#undef N
}

static char * test_hll_dense(void)
{
#define N 100
    long long n = 0, act;
    hll_t *dense;

    hll_add(&hll, "f", 1);
    dense = hll_sparse_to_dense(hll);
    pu_assert_not_null("conversion ok", dense);
    pu_assert_ptr_not_equal("converted", dense, hll);
    hll = dense;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < 2; j++) {
            char buf[80];

            snprintf(buf, sizeof(buf), "%d", i + j);

            hll_add(&hll, buf, strlen(buf));
            n++;
        }
    }

    act = hll_count(hll);
    pu_assert("count correct", WITHIN_ERR(act, N, 0.02));

    return NULL;
#undef N
}

static char * test_hll_dense2(void)
{
#define N 100
    long long n = 0, act;

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < 2; j++) {
            char buf[80];

            snprintf(buf, sizeof(buf), "%d", i + j);

            hll_add(&hll, buf, strlen(buf));
            n++;
        }
    }

    hll_t *dense = hll_sparse_to_dense(hll);
    pu_assert_not_null("conversion ok", dense);
    pu_assert_ptr_not_equal("converted", dense, hll);
    hll = dense;

    act = hll_count(hll);
    pu_assert("count correct", WITHIN_ERR(act, N, 0.02));

    return NULL;
#undef N
}

void all_tests(void)
{
    pu_def_test(test_hll_sparse, PU_RUN);
    pu_def_test(test_hll_huge, PU_RUN);
    pu_def_test(test_hll_dense, PU_RUN);
    pu_def_test(test_hll_dense2, PU_RUN);
}
