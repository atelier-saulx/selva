#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
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

int get_array_field_index(const char *field_str, size_t field_len, ssize_t *res) {
    if (field_str[field_len - 1] != ']') {
        return -1;
    }

    if (field_len < 3) {
        return -2;
    }

    for (ssize_t i = field_len - 2; i > 0; i--) {
        if (field_str[i] == '[') {
            ssize_t v;

            v = (ssize_t)strtol(field_str + i + 1, NULL, 10);

            if (res) {
                *res = v;
            }
            return 0;
        }
    }

    return -2;
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
