/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <tgmath.h>
#include <time.h>
#include "util/timestamp.h"

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
