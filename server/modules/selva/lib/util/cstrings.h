/*
 * Copyright (c) 2020-2021 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _UTIL_CSTRINGS_H_
#define _UTIL_CSTRINGS_H_

#include <stddef.h>

/**
 * Locate last occurrence of character in string.
 */
int strrnchr(const char *str, size_t len, char c);

int stringlist_search(const char *list, const char *str);
int stringlist_searchn(const char *list, const char *str, size_t n);
size_t substring_count(const char *string, const char *substring, size_t n);

/**
 * Find the first occurrence of find in s, where the search is limited to the
 * first slen characters of s.
 */
char * strnstr(const char *s, const char *find, size_t slen);

int get_array_field_index(const char *field_str, size_t field_len, ssize_t *res);

/**
 * Calculate the number of instances of ch in s.
 */
int ch_count(const char *s, char ch);

/**
 * Replace all occurrences of orig_ch in s with new_ch.
 */
char *ch_replace(char *s, size_t n, char orig_ch, char new_ch);

#ifndef HAS_MEMRCHR
void *memrchr(const void *s, int c, size_t n);
#endif

#endif /* _UTIL_CSTRINGS_H_ */
