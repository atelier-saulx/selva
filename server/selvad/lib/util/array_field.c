/*
 * Copyright (c) 2020-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "util/svector.h"
#include "util/array_field.h"

static int is_valid_index_notation(const char *field_str, ptrdiff_t i)
{
    /*
     * This won't catch all abuse but at least it fails on the most obvious
     * seemingly correct but unsupported notation.
     */
    if (i == 0 || /* "[0]" is not a valid filed name. */
        field_str[i - 1] == ']') /* no multi-dimensional arrays */
    {
        return 0;
    }

    /*
     * Note that currently a field name ending with a single dot ('.') is
     * considered valid.
     */

    return 1;
}

ssize_t get_array_field_index(const char *field_str, size_t field_len, ssize_t *res)
{
    const char *si;
    char *end;
    ptrdiff_t pos;
    ssize_t i;

    if (field_len < 3 || field_str[field_len - 1] != ']') {
        return 0;
    }

    si = memrchr(field_str, '[', field_len - 2);
    if (!si) {
        return -1;
    }

    pos = si - field_str;

    if (!is_valid_index_notation(field_str, pos)) {
        return -1;
    }

    i = (ssize_t)strtoll(si + 1, &end, 10);
    if (end != field_str + field_len - 1) {
        return -1;
    }
    if (res) {
        *res = i;
    }

    return pos;
}

size_t ary_idx_to_abs(ssize_t len, ssize_t ary_idx)
{
    if (ary_idx >= 0) {
        return ary_idx;
    } else if (len == 0) {
        return 0;
    } else {
        return imaxabs((len + ary_idx) % len);
    }
}

size_t vec_idx_to_abs(SVector *vec, ssize_t ary_idx)
{
    ssize_t len;

    if (ary_idx >= 0) {
        return ary_idx;
    }

    len = SVector_Size(vec);
    return len == 0 ? 0 : imaxabs((len + ary_idx) % len);
}
