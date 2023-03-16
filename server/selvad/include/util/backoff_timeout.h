/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct backoff_timeout;

struct backoff_timeout {
    double t_min; /*!< Min time. [ms] */
    double t_max; /*!< Max time. [ms] */
    double factor;
    int attempt; /*!< Current attempt nr. */
#ifndef __APPLE__
    struct random_data rnd_state;
#endif
    char rnd_state_buf[32];
};

extern const struct backoff_timeout backoff_timeout_defaults;

void backoff_timeout_init(struct backoff_timeout *s);
void backoff_timeout_next(struct backoff_timeout *s, struct timespec *ts);
