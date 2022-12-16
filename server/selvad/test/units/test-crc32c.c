/*
 * Copyright (c) 2022 SAULX
 *
 * SPDX-License-Identifier: MIT
 */

#include <punit.h>
#include <stdint.h>
#include "util/crc32c.h"

static void setup(void)
{
}

static void teardown(void)
{
}

static char * test_check(void)
{
    const char check1[] = "123456789";
    const char check2[] = "Hello world!";

    pu_assert_equal("should match", crc32c(0, check1, sizeof(check1) - 1), 0xe3069283);
    pu_assert_equal("should match", crc32c(0, check2, sizeof(check2) - 1), 0x7b98e751);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_check, PU_RUN);
}
