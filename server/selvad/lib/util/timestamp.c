/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <tgmath.h>
#include <time.h>
#include "util/timestamp.h"

#if __MACH__ || __APPLE__
#define MONOTIME_SOURCE CLOCK_MONOTONIC
#elif defined(CLOCK_MONOTONIC_COARSE)
#define MONOTIME_SOURCE CLOCK_MONOTONIC_COARSE
#elif _POSIX_MONOTONIC_CLOCK > 0
#define MONOTIME_SOURCE CLOCK_MONOTONIC
#else
#define MONOTIME_SOURCE CLOCK_REALTIME
#endif

#if __linux__
#define TAI_SOURCE CLOCK_TAI
#else
#define TAI_SOURCE CLOCK_REALTIME
#endif

long long ts_now(void) {
    struct timespec ts;
    long long now;

    /*
     * Don't bother checking the error because it never fails
     * in a working system.
     */
    clock_gettime(CLOCK_REALTIME, &ts);
    now = ts.tv_sec * 1000 + lround((double)ts.tv_nsec / 1.0e6);

    return now;
}

void ts_monotime(struct timespec *spec)
{
    clock_gettime(MONOTIME_SOURCE, spec);
}

void ts_monorealtime(struct timespec *spec)
{
    clock_gettime(TAI_SOURCE, spec);
}

long long ts_monorealtime_now(void)
{
    struct timespec ts;
    long long now;

    clock_gettime(TAI_SOURCE, &ts);
    now = ts.tv_sec * 1000 + lround((double)ts.tv_nsec / 1.0e6);

    return now;
}
