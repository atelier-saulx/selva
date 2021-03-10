#pragma once
#ifndef _UTIL_CSTRINGS_H_
#define _UTIL_CSTRINGS_H_

#include <stddef.h>

int stringlist_search(const char *list, const char *str);
int stringlist_searchn(const char *list, const char *str, size_t n);
size_t substring_count(const char *string, const char *substring, size_t n);
char * strnstr(const char *s, const char *find, size_t slen);

#endif /* _UTIL_CSTRINGS_H_ */
