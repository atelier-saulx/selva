/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include "util/ctime.h"
#include "util/timestamp.h"
#include "selva_log.h"
#include "event_loop.h"
#include "../event_loop_state.h"
#include "poll.h"

#ifndef USE_POLL
#error "I only support poll"
#endif

void evl_poll_init(struct event_loop_state *state __unused)
{
}

void evl_poll_deinit(struct event_loop_state *state __unused)
{
}

int evl_poll_add_fd(struct event_loop_state *state, int fd, enum event_type mask)
{
    state->fds[fd].mask |= mask;
    return 0;
}

void evl_poll_del_fd(struct event_loop_state *state, int fd, enum event_type mask)
{
    state->fds[fd].mask &= ~mask;
}

__unused static void print_events(const struct pollfd *pfd)
{
    typeof(pfd->revents) f = pfd->revents;
    const char *nfo[10];
#define SET_NFO(i, v) nfo[i] = (f & (v)) ? " " #v : ""
    SET_NFO(0, POLLERR);
    SET_NFO(1, POLLHUP);
    SET_NFO(2, POLLIN);
    SET_NFO(3, POLLNVAL);
    SET_NFO(4, POLLOUT);
    SET_NFO(5, POLLPRI);
    SET_NFO(6, POLLRDBAND);
    SET_NFO(7, POLLRDNORM);
    SET_NFO(8, POLLWRBAND);
    SET_NFO(9, POLLWRNORM);
#undef SET_NFO

    SELVA_LOG(SELVA_LOGL_DBG, "event for fd: %d revents:%s%s%s%s%s%s%s%s%s%s",
              pfd->fd,
              nfo[0], nfo[1], nfo[2], nfo[3], nfo[4],
              nfo[5], nfo[6], nfo[7], nfo[8], nfo[9]);
}

void evl_poll(struct event_loop_state *state, const struct timespec *timeout)
{
    int nfds = 0, nfds_out, itim;
    struct timespec expire, cur;
    const struct timespec sleepy = {
        .tv_nsec = 500,
    };

    ts_monotime(&expire);
    if (timeout) {
        itim = timeout->tv_sec + (int)(timeout->tv_nsec / 1000000000);
        timespec_add(&expire, &expire, timeout);
    } else {
        itim = -1;
    }

    for (int fd = 0; fd < EVENT_LOOP_MAX_FDS; fd++) {
        enum event_type mask = state->fds[fd].mask & (EVENT_TYPE_FD_READABLE | EVENT_TYPE_FD_WRITABLE);

        if (mask) {
            state->pfds[nfds++] = (struct pollfd){
                .fd = fd,
                .events = ((mask & EVENT_TYPE_FD_READABLE) ? POLLIN : 0) |
                          ((mask & EVENT_TYPE_FD_WRITABLE) ? POLLOUT : 0),
            };
        }
    }

    do {
        /* TODO EINTR might affect the timeout. */
        while ((nfds_out = poll(state->pfds, nfds, itim)) == -1 &&
               (errno == EINTR || errno == EAGAIN)) {
            continue;
        }

        /*
         * MacOS has a quirky but technically correct implementation of poll()
         * that may return prematurely if no fds are ready. Hence, we need to
         * implement our own timeout and retry logic here.
         */
        ts_monotime(&cur);
    } while (nfds_out == 0 &&
             timespec_cmp(&cur, &expire, <) &&
             (nanosleep(&sleepy, NULL), 1));
    if (nfds_out > 0) {
        const int offset = state->nr_pending;
        int nr_events = 0;

        for (int i = 0; i < nfds; i++) {
            const struct pollfd *pfd = &state->pfds[i];
            const int fd = pfd->fd;
            struct fd_reg *fe = &state->fds[fd];
            enum event_type mask = EVENT_TYPE_NONE;

#if 0
            print_events(pfd);
#endif

            if (fe->mask == EVENT_TYPE_NONE) {
                continue;
            }

            mask |= !(pfd->revents & POLLIN) ? 0 : EVENT_TYPE_FD_READABLE;
            mask |= !(pfd->revents & POLLOUT) ? 0 : EVENT_TYPE_FD_WRITABLE;
            mask |= !(pfd->revents & POLLHUP) ? 0 : EVENT_TYPE_FD_READABLE | EVENT_TYPE_FD_WRITABLE;

            state->pending[offset + nr_events].mask = mask;
            state->pending[offset + nr_events].fd = fd;

            if (nr_events + 1 == (int)num_elem(state->pending)) {
                break;
            }

            nr_events++;
        }

        state->nr_pending += nr_events;
    } else if (nfds_out < 0) {
        /*
         * TODO
         * - EFAULT
         * - EINVAL
         */
    }
}
