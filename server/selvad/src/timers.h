/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "queue.h"
#include "heap.h"
#include "event_loop.h"

struct timer_reg {
    struct timespec timeout;
    evl_event_cb cb; /*!< If NULL the timer is not registered. */
    void *arg;
    SLIST_ENTRY(timer_reg) next_free;
};

struct timers {
    struct timer_reg tim[EVENT_LOOP_MAX_TIMERS];
    SLIST_HEAD(timers_free, timer_reg) tim_free; /*!< Free timers. */
    HEAP_DEF(tim_queue, EVENT_LOOP_MAX_TIMERS); /*!< Waiting timers. */
};

void evl_timers_init(struct timers *timers);
int evl_timers_set_timeout(struct timers *timers, const struct timespec * restrict timeout, evl_event_cb cb, void *arg);
void evl_timers_clear_timeout(struct timers *timers, int tim_id);
int evl_timers_nr_waiting(struct timers *timers);
void evl_timers_tick(struct timers *timers);
struct timespec *evl_timers_next_timeout(struct timers *timers, struct timespec *spec);
