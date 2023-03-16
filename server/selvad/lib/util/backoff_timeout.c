/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "util/ctime.h"
#include "util/backoff_timeout.h"

const struct backoff_timeout backoff_timeout_defaults = {
    .t_min = 500.0,
    .t_max = 3000.0,
    .factor = 1.5,
};

void backoff_timeout_init(struct backoff_timeout *s)
{
    unsigned int seed = time(NULL);
#if __APPLE__
    initstate(seed, s->rnd_state_buf, sizeof(s->rnd_state_buf));
#else
    initstate_r(seed, s->rnd_state_buf, sizeof(s->rnd_state_buf), &s->rnd_state);
#endif
    s->attempt = 0;
}

#ifndef __APPLE__
static int get_rnd(struct random_data *rnd_state)
{
    int32_t r;

    (void)random_r(rnd_state, &r);

    return (int)r;
}
#endif

void backoff_timeout_next(struct backoff_timeout *s, struct timespec *ts)
{
    double rv;
    double timeout;

#if __APPLE__
    rv = (double)(random() % 100) / 100.0 + 1.0;
#else
    rv = (double)(get_rnd(&s->rnd_state) % 100) / 100.0 + 1.0;
#endif
    timeout = fmin(rv * s->t_min * pow(s->factor, (double)s->attempt), s->t_max * (1.0 / rv));
    s->attempt++;

    msec2timespec(ts, (int64_t)round(timeout));
}
