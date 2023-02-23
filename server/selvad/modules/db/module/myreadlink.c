/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include "selva_error.h"
#include "util/selva_string.h"
#include "myreadlink.h"

struct selva_string *myreadlink(const char *linkname)
{
    struct stat sb;
    struct selva_string *s;
    ssize_t nbytes, bufsiz;

    if (lstat(linkname, &sb) == -1) {
        return NULL;
    }

    if (sb.st_size == 0) {
        return NULL;
    }

    /*
     * Add one to the link size, so that we can determine whether
     * the buffer returned by readlink() was truncated.
     */
    bufsiz = sb.st_size + 1;
    s = selva_string_create(NULL, bufsiz, SELVA_STRING_MUTABLE);

    nbytes = readlink(linkname, selva_string_to_mstr(s, NULL), bufsiz);
    if (nbytes == -1 || nbytes == bufsiz) {
        return NULL;
    }

    selva_string_truncate(s, sb.st_size);
    return s;
}
