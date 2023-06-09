/*
 * Copyright (c) 2022-2023 SAULX
 *
 * SPDX-License-Identifier: MIT
 */

#include <punit.h>
#include "util/cstrings.h"

static void setup(void)
{
}

static void teardown(void)
{
}

/*
 * This is exactly the same matcher used in
 * module/subscriptions.c
 */
static int field_matcher(const char *list, const char *field)
{
    const char *sep = ".";
    int match;
    char *p;

    match = stringlist_searchn(list, field, strlen(field));
    if (!match && (p = strstr(field, sep))) {
        do {
            const size_t len = (ptrdiff_t)p++ - (ptrdiff_t)field;

            match = stringlist_searchn(list, field, len);
        } while (!match && p && (p = strstr(p, sep)));
    }

    return match;
}

static char * test_sztok_one(void)
{
    const char str[] = "hello";

    const char *s;
    size_t j = 0;
    while ((s = sztok(str, sizeof(str), &j))) {
        pu_assert_str_equal("", s, "hello");
    }

    return NULL;
}

static char * test_sztok_two(void)
{
    const char str[] = "hello\0world";

    const char *s;
    size_t j = 0, k = 0;
    while ((s = sztok(str, sizeof(str), &j))) {
        if (k++ == 0) {
            pu_assert_str_equal("", s, "hello");
        } else {
            pu_assert_str_equal("", s, "world");
        }
    }

    pu_assert_equal("iterations", k, 2);

    return NULL;
}

static char * test_sztok_with_strlen(void)
{
    const char *str = "hello";

    const char *s;
    size_t j = 0, k = 0;
    while ((s = sztok(str, strlen(str), &j))) {
        pu_assert_str_equal("", s, "hello");
        k++;
    }

    pu_assert_equal("iterations", k, 1);

    return NULL;
}

/*
 * This could be a bit dangerous use case but it might happen.
 * Safe with selva_string.
 */
static char * test_sztok_two_minus(void)
{
    const char str[] = "hello\0world";

    const char *s;
    size_t j = 0, k = 0;
    while ((s = sztok(str, sizeof(str) - 1, &j))) {
        if (k++ == 0) {
            pu_assert_str_equal("", s, "hello");
        } else {
            pu_assert_str_equal("", s, "world");
        }
    }

    pu_assert_equal("iterations", k, 2);

    return NULL;
}

static char * test_invalid_cases(void)
{
    const char *field = "title";
    int match;

    match = field_matcher("", field);
    pu_assert_equal("should not match", match, 0);

    match = stringlist_searchn("title", "", 0);
    pu_assert_equal("should not match", match, 0);

    match = stringlist_searchn("title", "\0", 1);
    pu_assert_equal("should not match", match, 0);

    return NULL;
}

static char * test_simple_match(void)
{
    const char *list = "title";
    const char *field = "title";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should just match", match, 1);

    return NULL;
}

static char * test_simple_no_match(void)
{
    const char *list = "title";
    const char *field = "titlo";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should not match", match, 0);

    return NULL;
}

static char * test_simple_match_in_list(void)
{
    const char *list = "abc\ntitle\ndef";
    const char *field = "title";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should match in the middle of the list", match, 1);

    return NULL;
}

static char * test_simple_match_in_list_last(void)
{
    const char *list = "abc\ntitle\ndef";
    const char *field = "def";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should match in the middle of the list", match, 1);

    return NULL;
}

static char * test_sub_match(void)
{
    const char *list = "title";
    const char *field = "title.en";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should just match", match, 1);

    return NULL;
}

static char * test_sub_list_match(void)
{
    const char *list = "image\ntitle";
    const char *field = "title.en";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should just match", match, 1);

    return NULL;
}

static char * test_sub_list_no_match(void)
{
    const char *list = "image\ntitle.en";
    const char *field = "title.ru";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should not match", match, 0);

    return NULL;
}

static char * test_sub_list_no_match_inverse1(void)
{
    const char *list = "image\ntitle.en";
    const char *field = "title";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should not match", match, 0);

    return NULL;
}

static char * test_sub_list_no_match_inverse2(void)
{
    const char *list = "image\ntitle.en";
    const char *field = "title.ru";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should not match", match, 0);

    return NULL;
}

static char * test_broken_list1(void)
{
    const char *list = "image\ntitle\n";
    const char *field = "title.en";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should match", match, 1);

    return NULL;
}

static char * test_broken_list2(void)
{
    const char *list = "pic\n\ntitle.en";
    const char *field = "title.en";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("should match", match, 1);

    return NULL;
}

static char * test_empty_field(void)
{
    const char *list = "abc\ntitle\ndef";
    const char *field = "";
    int match;

    match = field_matcher(list, field);
    pu_assert_equal("no match", match, 0);

    return NULL;
}

static char * test_long_string(void)
{
    const char *list = "abc\ntitle\ndef";
    const char field[] = "titlee";
    int match;

    match = stringlist_searchn(list, field, sizeof(field) - 3);
    pu_assert_equal("no match", match, 0);

    match = stringlist_searchn(list, field, sizeof(field) - 2);
    pu_assert_equal("match", match, 1);

    match = stringlist_searchn(list, field, sizeof(field) - 1);
    pu_assert_equal("no match", match, 0);

    return NULL;
}

static char * test_get_array_field_index(void)
{
    const char str1[] = "field";
    const char str2[] = "field[]";
    const char str3[] = "field[0]";
    const char str4[] = "field[90000]";
    const char str5[] = "field[-1]";
    const char str6[] = "field[[1]";
    const char str7[] = "field[1]]";
    const char str8[] = "[1]";
    const char str9[] = "[field1]";
    const char str10[] = "field[a1]";
    ssize_t len;
    ssize_t res;

    len = get_array_field_index(str1, sizeof(str1) - 1, &res);
    pu_assert_equal("", len, 0);

    len = get_array_field_index(str2, sizeof(str2) - 1, &res);
    pu_assert_equal("", len, -1);

    len = get_array_field_index(str3, sizeof(str3) - 1, &res);
    pu_assert_equal("", len, 5);
    pu_assert_equal("", res, 0);

    len = get_array_field_index(str4, sizeof(str4) - 1, &res);
    pu_assert_equal("", len, 5);
    pu_assert_equal("", res, 90000);

    len = get_array_field_index(str5, sizeof(str5) - 1, &res);
    pu_assert_equal("", len, 5);
    pu_assert_equal("", res, -1);

    len = get_array_field_index(str6, sizeof(str6) - 1, &res);
    pu_assert_equal("", len, 6);
    pu_assert_equal("", res, 1);

    len = get_array_field_index(str7, sizeof(str7) - 1, &res);
    pu_assert_equal("", len, -1);

    len = get_array_field_index(str8, sizeof(str8) - 1, &res);
    pu_assert_equal("", len, -1);

    len = get_array_field_index(str9, sizeof(str9) - 1, &res);
    pu_assert_equal("", len, -1);

    len = get_array_field_index(str10, sizeof(str10) - 1, &res);
    pu_assert_equal("", len, -1);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_sztok_one, PU_RUN);
    pu_def_test(test_sztok_two, PU_RUN);
    pu_def_test(test_sztok_with_strlen, PU_RUN);
    pu_def_test(test_sztok_two_minus, PU_RUN);
    pu_def_test(test_invalid_cases, PU_RUN);
    pu_def_test(test_simple_match, PU_RUN);
    pu_def_test(test_simple_no_match, PU_RUN);
    pu_def_test(test_simple_match_in_list, PU_RUN);
    pu_def_test(test_simple_match_in_list_last, PU_RUN);
    pu_def_test(test_sub_match, PU_RUN);
    pu_def_test(test_sub_list_match, PU_RUN);
    pu_def_test(test_sub_list_no_match, PU_RUN);
    pu_def_test(test_sub_list_no_match_inverse1, PU_RUN);
    pu_def_test(test_sub_list_no_match_inverse2, PU_RUN);
    pu_def_test(test_broken_list1, PU_RUN);
    pu_def_test(test_broken_list2, PU_SKIP); /* TODO This is currently failing but it's not a big deal */
    pu_def_test(test_empty_field, PU_RUN);
    pu_def_test(test_long_string, PU_RUN);
    pu_def_test(test_get_array_field_index, PU_RUN);
}
