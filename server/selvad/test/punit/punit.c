/**
 * @file punit.c
 * @brief PUnit, a portable unit testing framework for C.
 *
 * Inspired by: http://www.jera.com/techinfo/jtns/jtn002.html
 *
 * Copyright (c) 2012, Ninjaware Oy, Olli Vanhoja <olli.vanhoja@ninjaware.fi>
 * Copyright (c) 2913, Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
 * Copyright (c) 2022 SAULX
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/** @addtogroup PUnit
  * @{
  */

#include <stdio.h>
#include "cdefs.h"
#include "punit.h"

const char * const selva_db_version = "unittest";

/* Variables below are documented in punit.h */
int pu_tests_passed = 0;
int pu_tests_skipped = 0;
int pu_tests_count = 0;

/**
 * Test module description.
 * @param str a test module description string.
 */
void pu_mod_description(char * str)
{
#if PU_REPORT_ORIENTED == 1
    printf("Test module: %s\n", str);
#endif
}

/**
 * Test case description.
 * @param str a test case description string.
 */
void pu_test_description(char * str)
{
#if PU_REPORT_ORIENTED == 1
    printf("\t%s\n", str);
#endif
}

/**
 * Run PUnit tests.
 * This should be called in main().
 * @param all_tests pointer to a function containing actual test calls.
 */
int pu_run_tests(void (*all_tests)(void))
{
    all_tests();
    if (pu_tests_passed == pu_tests_count) {
        printf("ALL TESTS PASSED\n");
    }

    printf("Test passed: %d/%d, skipped: %d\n\n",
        pu_tests_passed, pu_tests_count, pu_tests_skipped);

    return (pu_tests_passed + pu_tests_skipped) != pu_tests_count;
}

int main(int argc __unused, char **argv __unused)
{
    void all_tests();
    return pu_run_tests(&all_tests);
}

/**
  * @}
  */
