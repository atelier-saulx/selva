/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "ctime.h"
#include "selva_error.h"
#include "selva_log.h"
#include "event_loop_state.h"
#include "poll.h"

struct event_loop_state event_loop_state;

/*
 * Handle stage pending 2 event callbacks.
 */
static void handle_pending_event(struct event *ev)
{
    const enum event_type mask = ev->mask;

    if (mask & (EVENT_TYPE_FD_READABLE | EVENT_TYPE_FD_WRITABLE)) {
        const int fd = ev->fd;
        struct fd_reg *fdr = &event_loop_state.fds[fd];

        if ((mask & EVENT_TYPE_FD_READABLE) && fdr->rd_cb) {
            fdr->rd_cb(ev, fdr->arg);
        }
        if ((mask & EVENT_TYPE_FD_WRITABLE) && fdr->wr_cb) {
            fdr->wr_cb(ev, fdr->arg);
        }
    }
}

void evl_init(void)
{
    memset(&event_loop_state, 0, sizeof(event_loop_state));

    event_loop_state.state = EVENT_LOOP_RUN_STATE_RUN;
    evl_poll_init(&event_loop_state);
    evl_timers_init(&event_loop_state.timers);
    event_loop_init_promises();
}

void evl_deinit(void)
{
    evl_poll_deinit(&event_loop_state);
}

int evl_set_timeout(const struct timespec * restrict timeout, evl_event_cb cb, void *arg)
{
    return evl_timers_set_timeout(&event_loop_state.timers, timeout, cb, arg);
}

void evl_clear_timeout(int timer_id)
{
    evl_timers_clear_timeout(&event_loop_state.timers, timer_id);
}

static int get_fdr(struct fd_reg **fdr, int fd)
{
    if (fd < 0) {
        return SELVA_EINVAL;
    }

    if (fd >= EVENT_LOOP_MAX_FDS) {
        return SELVA_ENOBUFS;
    }

    *fdr = &event_loop_state.fds[fd];
    return 0;
}

int evl_wait_fd(int fd, evl_event_cb rd_cb, evl_event_cb wr_cb, evl_event_cb close_cb, void *arg)
{
    enum event_type prev_mask;
    const enum event_type mask = (!rd_cb ? 0 : EVENT_TYPE_FD_READABLE) |
                                 (!wr_cb ? 0 : EVENT_TYPE_FD_WRITABLE) |
                                 (!close_cb ? 0 : EVENT_TYPE_FD_CLOSE);
    int err;
    struct fd_reg *fdr;

    if (!(rd_cb || wr_cb)) {
        return SELVA_EINVAL;
    }

    err = get_fdr(&fdr, fd);
    if (err) {
        return err;
    }

    prev_mask = fdr->mask;
    err = evl_poll_add_fd(&event_loop_state, fd, mask);
    if (err) {
        return err;
    }

    if (prev_mask == EVENT_TYPE_NONE) {
        fdr->rd_cb = NULL;
        fdr->wr_cb = NULL;
        fdr->close_cb = NULL;
        fdr->arg = NULL;

        event_loop_state.nr_fds++;
    }

    if (rd_cb) {
        fdr->rd_cb = rd_cb;
    }
    if (wr_cb) {
        fdr->wr_cb = wr_cb;
    }
    if (close_cb) {
        fdr->close_cb = close_cb;
    }
    if (arg) {
        fdr->arg = arg;
    }

    return 0;
}

int evl_end_fd(int fd)
{
    struct fd_reg *fdr;

    int err = get_fdr(&fdr, fd);
    if (err) {
        return err;
    }

    if (fdr->mask == EVENT_TYPE_NONE) {
        return SELVA_EINVAL;
    }

    if (event_loop_state.nr_pending_fd_close >= EVENT_LOOP_MAX_FDS) {
        return SELVA_ENOBUFS;
    }

    evl_poll_del_fd(&event_loop_state, fd, fdr->mask);

    /*
     * Stop handling of pending events for this fd immediately.
     * Any pending events will be discarded.
     */
    fdr->rd_cb = NULL;
    fdr->wr_cb = NULL;

    /*
     * The fdr will be freed after the close event is handled.
     * This will ensure that we can properly discard any pending events for the
     * file.
     */
    event_loop_state.pending_fd_close[event_loop_state.nr_pending_fd_close++] = (struct event){
        .mask = EVENT_TYPE_FD_CLOSE,
        .fd = fd,
    };

    return 0;
}

/**
 * Handle close event for an fd no longer monitored.
 * Call 'close' event for fd that we the user had stopped to listen with
 * evl_end_fd() and free the fdr.
 */
static void handle_fd_close_event(struct event *ev)
{
    struct fd_reg *fdr;
    const int fd = ev->fd;
    int err;

    err = get_fdr(&fdr, fd);
    if (unlikely(err)) {
        /* TODO Log error */
        return;
    }

    if (fdr->close_cb) {
        fdr->close_cb(ev, fdr->arg);
    } else {
        /* Otherwise close the file/socket. */
        struct stat statbuf;

        fstat(fd, &statbuf);
        if (S_ISSOCK(statbuf.st_mode)) {
            (void)shutdown(fd, SHUT_RDWR);
        }
        close(fd);
    }

    memset(fdr, 0, sizeof(*fdr));
    event_loop_state.nr_fds--;
}

void evl_start(void)
{
    const struct timespec ref = { 0 };

    /*
     * The Event Loop.
     */
    while (event_loop_state.state == EVENT_LOOP_RUN_STATE_RUN) {
        struct timespec timeout;

        /*
         * 1. Handle expiring timers.
         */
        evl_timers_tick(&event_loop_state.timers);

        /*
         * 2. Call pending event callbacks.
         */
        for (size_t i = 0; i < event_loop_state.nr_pending; i++) {
            handle_pending_event(&event_loop_state.pending[i]);
        }
        event_loop_state.nr_pending = 0;

        /*
         * 3. Handle resolved promises.
         */
        event_loop_handle_settled_promises();

        /*
         * 4. Poll I/O
         */
        if (event_loop_state.nr_fds > 0) {
            const int make_zero = event_loop_state.nr_pending_fd_close;
            struct timespec *ptimeout;

            if (make_zero) {
                memset(&timeout, 0, sizeof(timeout));
                ptimeout = &timeout;
            } else {
                ptimeout = evl_timers_next_timeout(&event_loop_state.timers, &timeout);
            }

            evl_poll(&event_loop_state, ptimeout);
        }
        if (event_loop_state.nr_pending == 0 &&
            event_loop_state.nr_pending_fd_close == 0 &&
            evl_timers_next_timeout(&event_loop_state.timers, &timeout) &&
            timespec_cmp(&timeout, &ref, >)) {
            /*
             * Sleep if no new events were defferred and there is still time
             * left until the next timer expires.
             */
            nanosleep(&timeout, NULL);
        }

        /*
         * 5. Handle close events.
         */
        for (size_t i = 0; i < event_loop_state.nr_pending_fd_close; i++) {
            handle_fd_close_event(&event_loop_state.pending_fd_close[i]);
        }
        event_loop_state.nr_pending_fd_close = 0;

        SELVA_LOG(SELVA_LOGL_DBG, "evl tick: %d %d %d %d",
               !!evl_timers_nr_waiting(&event_loop_state.timers),
               !!event_loop_state.nr_fds,
               !!event_loop_state.nr_pending,
               !!event_loop_state.async_nr_awaiting);
        /* Stop if nothing is waiting or pending anymore. */
        if (evl_timers_nr_waiting(&event_loop_state.timers) == 0 &&
            event_loop_state.nr_fds == 0 &&
            event_loop_state.nr_pending == 0 &&
            event_loop_state.async_nr_awaiting == 0) {
            event_loop_state.state = EVENT_LOOP_RUN_STATE_STOP;
            SELVA_LOG(SELVA_LOGL_INFO, "EVENT_LOOP_RUN_STATE_STOP");
        }
    }

    /* TODO Take care of still pending/fulfilled promises */
}

void evl_stop(void)
{
    event_loop_state.state = EVENT_LOOP_RUN_STATE_STOP;
}
