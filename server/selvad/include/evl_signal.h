/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdint.h>
#include "_evl_export.h"

struct evl_siginfo {
    int esi_signo; /*!< Signal number. */
    int esi_code; /*!< Signal code. */
    int esi_errno; /*!< errno. */
    pid_t esi_pid; /*!< PID of the sender. */
    uid_t esi_uid; /*!< Real user ID of sending process. */
    int esi_status; /*!< Exit status or signal (SIGCHLD). */
    clock_t esi_utime; /*!< User time consumed. (SIGCHLD) */
    clock_t esi_stime; /*!< System time consumed. (SIGCHLD) */
    uintptr_t esi_addr; /* Memory location which caused fault (for hardware-generated signals). */
    int esi_int; /* POSIX.1b signal, sigqueue(3). */
    void *esi_ptr; /* POSIX.1b signal, sigqueue(3). */
    long esi_band; /* Band event. */
};

EVL_EXPORT(int, evl_create_sigfd, sigset_t *mask);
EVL_EXPORT(int, evl_read_sigfd, struct evl_siginfo *esig, int sfd);

#define _evl_import_signal(apply) \
    apply(evl_create_sigfd) \
    apply(evl_read_sigfd)

/**
 * Import all symbols from event_loop.h.
 */
#define evl_import_signal() \
    _evl_import_signal(evl_import_main)
