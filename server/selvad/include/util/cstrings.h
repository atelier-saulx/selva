/*
 * Copyright (c) 2020-2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _UTIL_CSTRINGS_H_
#define _UTIL_CSTRINGS_H_

#include <stddef.h>

[[nodiscard]]
char *selva_strndup(const char *s, size_t n) __attribute__((access(read_only, 1, 2), returns_nonnull));

[[nodiscard]]
char *selva_strdup(const char *s) __attribute__((access(read_only, 1), returns_nonnull));

/**
 * Locate last occurrence of character in string.
 */
int strrnchr(const char *str, size_t len, char c) __attribute__((pure, access(read_only, 1, 2)));

int stringlist_search(const char *list, const char *str) __attribute__((pure, access(read_only, 1), access(read_only, 2)));
int stringlist_searchn(const char *list, const char *str, size_t n) __attribute__((pure, access(read_only, 1), access(read_only, 2, 3)));
size_t substring_count(const char *string, const char *substring, size_t n) __attribute__((pure, access(read_only, 1), access(read_only, 2, 3)));

/**
 * Filter strings by prefix and remove the prefix when inserting to dst.
 * @param dst must be large enough to fit src in the worst case.
 * @param prefix_str is an optional prefix.
 */
void stringlist_remove_prefix(char *dst, const char *src, int len, const char *prefix_str, size_t prefix_len) __attribute__((access(write_only, 1), access(read_only, 2, 3)));

ssize_t get_array_field_index(const char *field_str, size_t field_len, ssize_t *res) __attribute__((access(read_only, 1, 2), access(write_only, 3)));

/**
 * Calculate the number of instances of ch in s.
 */
int ch_count(const char *s, char ch) __attribute__((pure, access(read_only, 1)));

/**
 * Replace all occurrences of orig_ch in s with new_ch.
 */
char *ch_replace(char *s, size_t n, char orig_ch, char new_ch) __attribute__((access(read_write, 1, 2)));

#ifndef HAS_MEMRCHR
void *memrchr(const void *s, int c, size_t n) __attribute__((pure, access(read_only, 1, 3)));
#endif

#endif /* _UTIL_CSTRINGS_H_ */
