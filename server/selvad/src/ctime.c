/*
 * Copyright (c) 2022 Saulx
 * Copyright (c) 2014 - 2016, 2019 Olli Vanhoja <olli.vanhoja@alumni.helsinki.fi>
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include "ctime.h"

void nsec2timespec(struct timespec * ts, int64_t nsec)
{
    const int64_t sec_nsec = (int64_t)1000000000;
    int64_t mod;

    mod = nsec % sec_nsec;
    ts->tv_sec = (nsec - mod) / sec_nsec;
    ts->tv_nsec = mod;
}

void timespec_add(struct timespec * sum, const struct timespec * left,
                  const struct timespec * right)
{
    struct timespec ts;

    sum->tv_sec = left->tv_sec + right->tv_sec;
    nsec2timespec(&ts, (int64_t)left->tv_nsec + (int64_t)right->tv_nsec);
    sum->tv_sec += ts.tv_sec;
    sum->tv_nsec = ts.tv_nsec;
}

void timespec_sub(struct timespec * diff, const struct timespec * left,
                  const struct timespec * right)
{
    const int64_t sec_nsec = (int64_t)1000000000;
    struct timespec ts;

    diff->tv_sec = left->tv_sec - right->tv_sec;
    nsec2timespec(&ts, (int64_t)left->tv_nsec - (int64_t)right->tv_nsec);
    if (ts.tv_nsec < 0 && diff->tv_sec >= 1) {
        diff->tv_sec -= 1;
        ts.tv_nsec += sec_nsec;
    }
    diff->tv_sec += ts.tv_sec;
    diff->tv_nsec = ts.tv_nsec;
}

void timespec_mul(struct timespec * prod, const struct timespec * left,
                  const struct timespec * right)
{
    struct timespec ts;

    prod->tv_sec = left->tv_sec * right->tv_sec;
    nsec2timespec(&ts, (int64_t)left->tv_nsec * (int64_t)right->tv_nsec);
    prod->tv_sec += ts.tv_sec;
    prod->tv_nsec = ts.tv_nsec;
}

void timespec_div(struct timespec * quot, const struct timespec * left,
                  const struct timespec * right)
{
    quot->tv_sec = left->tv_sec / right->tv_sec;
    quot->tv_nsec = left->tv_nsec / right->tv_nsec;
}

void timespec_mod(struct timespec * rem, const struct timespec * left,
                  const struct timespec * right)
{
    rem->tv_sec = left->tv_sec % right->tv_sec;
    rem->tv_nsec = left->tv_nsec % right->tv_nsec;
}
