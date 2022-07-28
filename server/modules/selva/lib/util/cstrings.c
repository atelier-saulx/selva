/*
 * Copyright (c) 2020-2021 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#define _GNU_SOURCE
#include <string.h>
#include "cstrings.h"

/* int is probably large enough for Selva users. */
int strrnchr(const char *str, size_t len, char c) {
    int i = len;

    while (i > 0) {
        if (str[--i] == c) {
            break;
        }
    }

    return i;
}

int stringlist_search(const char *list, const char *str) {
    const char *s1 = list;

    while (*s1 != '\0') {
        const char *s2 = str;

        /* strcmp */
        while (*s1 == *s2++) {
            const char c = *s1++;
            if (c == '\n' || c == '\0') {
                return 1;
            }
        }
        if (*s1 == '\n' && *(s2 - 1) == '\0') {
            return 1;
        }

        /* Skip the rest of the current field */
        while (*s1 != '\0') {
            s1++;
            if (*s1 == '\n') {
                s1++;
                break;
            }
        }
    }

    return 0;
}

int stringlist_searchn(const char *list, const char *str, size_t n) {
    const char *s1 = list;

    if (!str || str[0] == '\0' || n == 0) {
        return 0;
    }

    while (*s1 != '\0') {
        ssize_t i = n;
        const char *s2 = str;

        while (i-- >= 0 && *s1 && *s2 && *s1++ == *s2++);
        --s1;
        --s2;

        if (i == (ssize_t)(-1) &&
            ((s1[0] == '\n' || s1[0] == '\0') ||
             (s1[1] == '\0' || s1[1] == '\n'))) {
            return 1;
        }

        /* Skip the rest of the current field */
        while (*s1 != '\0') {
            s1++;
            if (*s1 == '\n') {
                s1++;
                break;
            }
        }
    }

    return 0;
}

static char * prefixed_only_cpy(char *dst, const char *src, size_t len, const char *prefix_str, size_t prefix_len) {
    if (len > prefix_len && !strncmp(src, prefix_str, prefix_len)) {
        size_t cpy_len = len - prefix_len;

        memcpy(dst, src + prefix_len, cpy_len);
        return dst + cpy_len;
    }

    return dst;
}

void stringlist_remove_prefix(char *dst, const char *src, int len, const char *prefix_str, size_t prefix_len) {
    const char *s = src;

    if (len == 0) {
        return;
    }

    dst[0] = '\0';

    while (len > 0) {
        const char *end;

        end = memmem(s, len, "\n", 1);
        if (!end) {
            end = s + len;
        }

        const size_t slen = end - s;

        if (prefix_str && prefix_len > 0) {
            char *new_dst;

            new_dst = prefixed_only_cpy(dst, s, slen, prefix_str, prefix_len);
            if (new_dst != dst) {
                dst = new_dst;
                *(dst++) = '\n';
            }
        } else {
            memcpy(dst, s, slen);
            dst += slen;
            *(dst++) = '\n';
        }

        s += slen + 1;
        len -= slen + 1;
    }

    if (len <= 0) {
        *(--dst) = '\0';
    }
}

size_t substring_count(const char *string, const char *substring, size_t n) {
    size_t l1, l2;
    size_t count = 0;

    l1 = n;
    l2 = strlen(substring);

    for (size_t i = 0; i < l1 - l2; i++) {
        if (strstr(string + i, substring) == string + i) {
            count++;
            i = i + l2 - 1;
        }
    }

    return count;
}

ssize_t get_array_field_index(const char *field_str, size_t field_len, ssize_t *res) {
    const char *si;
    char *end;
    ssize_t i;

    if (field_str[field_len - 1] != ']') {
        return -1;
    }

    if (field_len < 3) {
        return -2;
    }

    si = memrchr(field_str, '[', field_len - 2);
    if (!si) {
        return -2;
    }

    i = (ssize_t)strtoll(si + 1, &end, 10);
    if (end != field_str + field_len - 1) {
        return -2;
    }
    if (res) {
        *res = i;
    }

    return (ssize_t)(si - field_str);
}

int ch_count(const char *s, char ch) {
    size_t i = 0;

    while (*s) {
        i = *s++ == ch ? i + 1 : i;
    }

    return i;
}

char *ch_replace(char *s, size_t n, char orig_ch, char new_ch) {
    char * const e = s + n;

    for (char *p = s, c = *s; p != e && c != '\0'; c = *++p) {
        if (c == orig_ch) {
            *p = new_ch;
        }
    }

    return s;
}
