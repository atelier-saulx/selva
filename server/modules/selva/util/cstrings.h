#pragma once
#ifndef _UTIL_CSTRINGS_H_
#define _UTIL_CSTRINGS_H_

#include <stddef.h>

int stringlist_search(const char *list, const char *str);
int stringlist_searchn(const char *list, const char *str, size_t n);

#endif /* _UTIL_CSTRINGS_H_ */
