/*
 * Copyright (c) 2023 SAULX
 *
 * SPDX-License-Identifier: MIT
 */

#include <punit.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cdefs.h"
#include "util/selva_string.h"
#include "util/finalizer.h"
#include "selva_proto.h"

static void setup(void)
{
}

static void teardown(void)
{
}

static char * test_scanf_basic(void)
{
    __auto_finalizer struct finalizer fin;
    finalizer_init(&fin);

    /*
     * [5.0, 42, "hello", [["hello", "world"], [1, 2, 3]]]
     */
    struct {
        struct selva_proto_double d;
        struct selva_proto_longlong ll;
        struct selva_proto_string s;
        char s_str[sizeof("hello") - 1];
        struct selva_proto_array container;
        struct selva_proto_array as;
        struct selva_proto_string as0;
        char as0_str[sizeof("hello") - 1];
        struct selva_proto_string as1;
        char as1_str[sizeof("world") - 1];
        /* TODO Embedded arrays are not supported */
#if 0
        struct selva_proto_array all;
        long long all0;
        long long all1;
        long long all2;
#endif
        struct selva_proto_longlong ll2;
    } __packed buf = {
        .d = {
            .type = SELVA_PROTO_DOUBLE,
            .v = 5.0,
        },
        .ll = {
            .type = SELVA_PROTO_LONGLONG,
            .v = 42,
        },
        .s = {
            .type = SELVA_PROTO_STRING,
            .bsize = sizeof(buf.s_str),
        },
        .s_str = { 'h', 'e', 'l', 'l', 'o' },
        .container = {
            .type = SELVA_PROTO_ARRAY,
            .length = 2,
        },
        .as = {
            .type = SELVA_PROTO_ARRAY,
            .length = 2,
        },
        .as0 = {
            .type = SELVA_PROTO_STRING,
            .bsize = sizeof(buf.as0_str),
        },
        .as0_str = { 'h', 'e', 'l', 'l', 'o' },
        .as1 = {
            .type = SELVA_PROTO_STRING,
            .bsize = sizeof(buf.as1_str),
        },
        .as1_str = { 'w', 'o', 'r', 'l', 'd' },
#if 0
        .all = {
            .type = SELVA_PROTO_ARRAY,
            .flags = SELVA_PROTO_ARRAY_FLONGLONG,
            .length = 3,
        },
        .all0 = 1,
        .all1 = 2,
        .all2 = 3,
#endif
        .ll2 = {
            .type = SELVA_PROTO_LONGLONG,
            .v = 7,
        },
    };

    double d;
    long long ll, ll2;
    size_t s_len;
    const char *s_str;
    struct selva_string *as0;
    struct selva_string *as1;
    int all[3];
    int argc = selva_proto_scanf(&fin, &buf, sizeof(buf), "%lf, %lld, %.*s, {{%p, %p}, %d}",
                                 &d,
                                 &ll,
                                 &s_len, &s_str,
                                 &as0, &as1,
                                 &ll2);

    pu_assert_equal("argc", argc, 6);
    pu_assert_equal("", d, 5.0);
    pu_assert_equal("", ll, 42);
    pu_assert("", s_len == 5 && !memcmp(s_str, "hello", 5));
    pu_assert_str_equal("", selva_string_to_str(as0, NULL), "hello");
    pu_assert_str_equal("", selva_string_to_str(as1, NULL), "world");
#if 0
    pu_assert_equal("", all[0], 1);
    pu_assert_equal("", all[1], 2);
    pu_assert_equal("", all[2], 3);
#endif
    pu_assert_equal("", ll2, 7);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_scanf_basic, PU_RUN);
}
