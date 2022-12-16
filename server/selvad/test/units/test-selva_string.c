/*
 * Copyright (c) 2022 SAULX
 *
 * SPDX-License-Identifier: MIT
 */

#include <punit.h>
#include <stddef.h>
#include <stdio.h>
#include "cdefs.h"
#include "util/selva_string.h"

static void setup(void)
{
}

static void teardown(void)
{
}

static char * test_create(void)
{
    struct selva_string *s;
    const char *str;
    size_t len;

    s = selva_string_create("hello world", 11, 0);
    pu_assert("a pointer is returned", s);

    str = selva_string_to_str(s, &len);
    pu_assert_str_equal("the string was set", str, "hello world");
    pu_assert_equal("length was returned correctly", len, 11);

    selva_string_free(s);

    return NULL;
}

static char * test_createf(void)
{
    struct selva_string *s;
    const char *str;
    size_t len;

    s = selva_string_createf("hello %s: %d", "world", 10);

    str = selva_string_to_str(s, &len);
    pu_assert_str_equal("the string was set", str, "hello world: 10");
    pu_assert_equal("length was returned correctly", len, 15);

    selva_string_free(s);

    return NULL;
}

static char * test_dup(void)
{
    struct selva_string *s1;
    struct selva_string *s2;
    const char *str;

    s1 = selva_string_create("hello world", 11, 0);
    pu_assert("created", s1);
    s2 = selva_string_dup(s1, 0);
    pu_assert("cloned", s2);
    str = selva_string_to_str(s2, NULL);
    pu_assert_str_equal("cloned string equals the original", str, "hello world");

    selva_string_free(s1);
    selva_string_free(s2);

    return NULL;
}

static char * test_truncate(void)
{
    struct selva_string *s;
    const char *str;

    s = selva_string_create("hello world", 11, SELVA_STRING_MUTABLE);
    pu_assert("a pointer is returned", s);
    selva_string_truncate(s, 5);
    str = selva_string_to_str(s, NULL);
    pu_assert_str_equal("string was truncated", str, "hello");

    selva_string_free(s);

    return NULL;
}

static char * test_append(void)
{
    struct selva_string *s;
    const char *str;

    s = selva_string_create("hello", 5, SELVA_STRING_MUTABLE);
    pu_assert("a pointer is returned", s);
    selva_string_append(s, " world", 6);
    str = selva_string_to_str(s, NULL);
    pu_assert_str_equal("string was appended", str, "hello world");

    selva_string_free(s);

    return NULL;
}

static char * test_replace(void)
{
    struct selva_string *s;
    const char *str;

    s = selva_string_create("uvw", 3, SELVA_STRING_MUTABLE);
    pu_assert("a pointer is returned", s);
    selva_string_replace(s, "xyz", 3);
    str = selva_string_to_str(s, NULL);
    pu_assert_str_equal("string was replaced", str, "xyz");

    selva_string_free(s);

    return NULL;
}

static char * test_crc(void)
{
    struct selva_string *s;
    const char *str;

    s = selva_string_create("hello", 5, SELVA_STRING_MUTABLE | SELVA_STRING_CRC);
    pu_assert("a pointer is returned", s);
    pu_assert_equal("CRC verifies", selva_string_verify_crc(s), 1);
    selva_string_append(s, " world", 6);
    pu_assert_equal("CRC verifies", selva_string_verify_crc(s), 1);
    str = selva_string_to_str(s, NULL);

    ((char *)str)[1] = 'a';
    pu_assert_equal("CRC fails", selva_string_verify_crc(s), 0);
    ((char *)str)[1] = 'e';
    pu_assert_equal("CRC fails", selva_string_verify_crc(s), 1);

    selva_string_free(s);

    return NULL;
}

static char * test_cmp(void)
{
    struct selva_string *s1;
    struct selva_string *s2;
    struct selva_string *s3;

    s1 = selva_string_create("abraham", 7, 0);
    s2 = selva_string_create("isaac", 5, 0);
    s3 = selva_string_create("hagar", 5, 0);

    pu_assert_equal("cmp works", selva_string_cmp(s1, s1), 0);
    pu_assert_equal("cmp works", selva_string_cmp(s1, s2), -8);
    pu_assert_equal("cmp works", selva_string_cmp(s1, s3), -7);
    pu_assert_equal("cmp works", selva_string_cmp(s2, s3), 1);

    selva_string_free(s1);
    selva_string_free(s2);
    selva_string_free(s3);

    return NULL;
}

static char * test_intern(void)
{
    struct selva_string *s1;
    struct selva_string *s2;

    s1 = selva_string_create("abraham", 7, SELVA_STRING_INTERN);
    s2 = selva_string_create("abraham", 7, SELVA_STRING_INTERN);

    pu_assert_ptr_equal("interned", s1, s2);

    selva_string_free(s1);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_create, PU_RUN);
    pu_def_test(test_createf, PU_RUN);
    pu_def_test(test_dup, PU_RUN);
    pu_def_test(test_truncate, PU_RUN);
    pu_def_test(test_append, PU_RUN);
    pu_def_test(test_replace, PU_RUN);
    pu_def_test(test_crc, PU_RUN);
    pu_def_test(test_cmp, PU_RUN);
    pu_def_test(test_intern, PU_RUN);
}
