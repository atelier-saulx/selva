#pragma once

#if __APPLE__
#define _XOPEN_SOURCE
#endif

#include <ucontext.h>
#include "queue.h"
#include "timers.h"
#include "event_loop.h"
#if USE_EPOLL
#include <sys/epoll.h>
#endif
#if USE_POLL
#include <poll.h>
#endif

enum event_loop_run_state {
    EVENT_LOOP_RUN_STATE_STOP = 0,
    EVENT_LOOP_RUN_STATE_RUN,
};

struct fd_reg {
    enum event_type mask;
    evl_event_cb rd_cb;
    evl_event_cb wr_cb;
    evl_event_cb close_cb;
    void *arg;
};

SLIST_HEAD(promise_list, evl_promise);
LIST_HEAD(evl_async_ctx_list, evl_async_ctx);

/**
 * Internal state struct.
 */
struct event_loop_state {
    enum event_loop_run_state state;

    /*
     * Timer event registrations.
     */
    struct timers timers;

    /*
     * File event registrations and handling.
     */
#if USE_EPOLL
    int epfd;
    struct epoll_event ep_events[EVENT_LOOP_MAX_EVENTS];
#elif USE_POLL
    struct pollfd pfds[EVENT_LOOP_MAX_FDS];
#else
#error "How Am I gonna poll fds?"
#endif
    struct fd_reg fds[EVENT_LOOP_MAX_FDS];
    size_t nr_fds;

    /**
     * Pending events.
     */
    struct event pending[EVENT_LOOP_MAX_EVENTS];
    size_t nr_pending; /*!< Number of pending events. */

    /**
     * Pending fd close events.
     */
    struct event pending_fd_close[EVENT_LOOP_MAX_FDS];
    size_t nr_pending_fd_close;

    /**
     * Promises.
     */
    ucontext_t async_uctx_main; /*!< Store the context of main() stack here. */
    struct evl_async_ctx_list async_ctx_free; /*!< Preallocated contexts free. */
    struct evl_async_ctx_list async_ctx_busy; /*!< Preallocated contexts in use. */
    struct promise_list async_settled_promises; /*!< Settled promises that still needs to be handled by the owner. */
    size_t async_nr_awaiting; /*!< Number of awaiting async functions. */
};

/**
 * The internal state of the event loop.
 */
extern struct event_loop_state event_loop_state;

/*
 * in promise.c
 */
void event_loop_init_promises(void);
void event_loop_handle_settled_promises(void);
