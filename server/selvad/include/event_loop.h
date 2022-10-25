/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "_evl_export.h"

#define EVENT_LOOP_MAX_TIMERS 10
#define EVENT_LOOP_MAX_FDS 100
#define EVENT_LOOP_MAX_EVENTS 64
#define EVENT_LOOP_MAX_ASYNC 1000 /*!< Maximum number of async function call contexts. */
#define EVENT_LOOP_ASYNC_STACK_SIZE 1048576 /*!< Async call stack size in bytes. */

struct timespec;

/**
 * Event type.
 */
enum event_type {
    EVENT_TYPE_NONE = 0x00,
    EVENT_TYPE_TIMER = 0x01,
    EVENT_TYPE_FD_READABLE = 0x10,
    EVENT_TYPE_FD_WRITABLE = 0x20,
    EVENT_TYPE_FD_CLOSE = 0x40,
};

/**
 * Event descriptor.
 */
struct event {
    enum event_type mask;
    union {
        int timer_id;
        int fd;
    };
};

typedef void (*evl_event_cb)(struct event *event, void *arg);

void evl_init(void);
void evl_deinit(void);
void evl_start(void);
EVL_EXPORT(int, evl_set_timeout, const struct timespec * restrict timeout, evl_event_cb cb, void *arg);
EVL_EXPORT(void, evl_clear_timeout, int timer_id);

/**
 * Listen for events for fd.
 * Can be called multiple times for the same fd to modify the callbacks.
 */
EVL_EXPORT(int, evl_wait_fd, int fd, evl_event_cb rd_cb, evl_event_cb wr_cb, evl_event_cb close_cb, void *arg);

/**
 * Stop receiving events for fd.
 */
EVL_EXPORT(int, evl_end_fd, int fd);

#define _evl_import_event_loop(apply) \
    apply(evl_set_timeout) \
    apply(evl_clear_timeout) \
    apply(evl_wait_fd) \
    apply(evl_end_fd)

/**
 * Import all symbols from event_loop.h.
 */
#define evl_import_event_loop() \
    _evl_import_event_loop(evl_import_main)
