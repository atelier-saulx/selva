#include <string.h>
#include <sys/types.h>
#include "cstrings.h"

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

		if (i == (size_t)(-1) &&
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

size_t substring_count(const char *string, const char *substring) {
	size_t i, l1, l2;
	size_t count = 0;

	l1 = strlen(string);
	l2 = strlen(substring);

	for (i = 0; i < l1 - l2; i++) {
		if (strstr(string + i, substring) == string + i) {
			count++;
			i = i + l2 - 1;
		}
	}

	return count;
}
