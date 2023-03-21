/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#if __linux__
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#if __APPLE__
extern const char * const sys_siglist[];
extern const char * const sys_signame[];
#endif

static __thread char buf[80];

const char *sigstr_abbrev(int signum)
{
#if __linux__
    snprintf(buf, sizeof(buf), "SIG%s", sigabbrev_np(signum));
#elif __APPLE__
    snprintf(buf, sizeof(buf), "SIG%s", sys_signame[signum]);

    for (char *s = buf; *s; s++) {
        *s = toupper((unsigned char) *s);
    }
#else
    snprintf(buf, size, "%d", signum);
#endif

    return buf;
}

const char *sigstr_descr(int signum)
{
#if __linux__
    return sigdescr_np(signum);
#elif __APPLE__
    return sys_siglist[signum];
#else
    return "";
#endif
}

