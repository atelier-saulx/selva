/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include "selva_error.h"
#include "event_loop.h"
#include "../event_loop_state.h"
#include "poll.h"

#ifndef USE_EPOLL
#error "I only support epoll"
#endif

void evl_poll_init(struct event_loop_state *state)
{
    memset(state->ep_events, 0, sizeof(state->ep_events));
    state->epfd = epoll_create1(EPOLL_CLOEXEC);
}

void evl_poll_deinit(struct event_loop_state *state)
{
    close(state->epfd);
}

int evl_poll_add_fd(struct event_loop_state *state, int fd, enum event_type mask)
{
    const int op = state->fds[fd].mask == EVENT_TYPE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    mask |= state->fds[fd].mask;
    struct epoll_event ee = {
        .events = (!(mask & EVENT_TYPE_FD_READABLE) ? 0 : EPOLLIN) |
                  (!(mask & EVENT_TYPE_FD_WRITABLE) ? 0 : EPOLLOUT),
        .data = {
            .fd = fd,
        },
    };
    int res;

    state->fds[fd].mask = mask;

    res = epoll_ctl(state->epfd, op, fd, &ee);
    switch (res) {
    case 0:
        return 0;
    case EBADF:
    case EINVAL:
    case ELOOP:
    case EPERM:
        return SELVA_EINVAL;
    case EEXIST:
        return SELVA_EEXIST;
    case ENOENT:
        return SELVA_ENOENT;
    case ENOMEM:
    case ENOSPC:
        return SELVA_ENOBUFS;
    default:
        return SELVA_EGENERAL;
    }
}

void evl_poll_del_fd(struct event_loop_state *state, int fd, enum event_type mask)
{
    mask = state->fds[fd].mask & ~mask;
    struct epoll_event ee = {
        .events = (!(mask & EVENT_TYPE_FD_READABLE) ? 0 : EPOLLIN) |
                  (!(mask & EVENT_TYPE_FD_WRITABLE) ? 0 : EPOLLOUT),
        .data = {
            .fd = fd,
        },
    };

    state->fds[fd].mask = mask;

    if (mask != EVENT_TYPE_NONE) {
        epoll_ctl(state->epfd, EPOLL_CTL_MOD, fd, &ee);
    } else {
        epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, NULL);
    }
}

void evl_poll(struct event_loop_state *state, const struct timespec *timeout)
{
    int res;
    const int max_events = min(
            num_elem(state->ep_events),
            (num_elem(state->pending) - state->nr_pending));

    if (max_events == 0) {
        return;
    }

	res = epoll_pwait2(state->epfd, state->ep_events, max_events, timeout, NULL /* sigset */);
	if (res > 0) {
        int nr_events = 0;
        const int offset = state->nr_pending;
		nr_events = res;

		for (int i = 0; i < nr_events; i++) {
            enum event_type mask = EVENT_TYPE_NONE;
			struct epoll_event *e = state->ep_events + i;

            mask |= !(e->events & EPOLLIN) ? 0 : EVENT_TYPE_FD_READABLE;
            mask |= !(e->events & EPOLLOUT) ? 0 : EVENT_TYPE_FD_WRITABLE;
            mask |= !(e->events & EPOLLERR) ? 0 : EVENT_TYPE_FD_WRITABLE | EVENT_TYPE_FD_READABLE;
            mask |= !(e->events & EPOLLHUP) ? 0 : EVENT_TYPE_FD_WRITABLE | EVENT_TYPE_FD_READABLE;

			state->pending[offset + i].mask = mask;
			state->pending[offset + i].fd = e->data.fd;

            if (i + 1 == num_elem(state->pending)) {
                /*
                 * Just to be sure.
                 * RFE What if there was something left?
                 */
                break;
            }
		}

        state->nr_pending += nr_events;
	}
}
