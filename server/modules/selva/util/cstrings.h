#pragma once
#ifndef _UTIL_CSTRINGS_H_
#define _UTIL_CSTRINGS_H_

#include <stddef.h>

int stringlist_search(const char *list, const char *str);
int stringlist_searchn(const char *list, const char *str, size_t n);
size_t substring_count(const char *string, const char *substring, size_t n);
char * strnstr(const char *s, const char *find, size_t slen);
int is_array_field(const char *field_str, size_t field_len);
int get_array_field_index(const char *field_str, size_t field_len);
int get_array_field_start_idx(const char *field_str, size_t field_len);
int ch_count(const char *s, char ch);

#endif /* _UTIL_CSTRINGS_H_ */
