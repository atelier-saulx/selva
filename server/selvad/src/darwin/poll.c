#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
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

void print_events(const struct pollfd *pfd)
{
    typeof(pfd->revents) f = pfd->revents;

    printf("event for fd: %d revents:", pfd->fd);
    if (f & POLLERR) printf(" POLLERR");
    if (f & POLLHUP) printf(" POLLHUP");
    if (f & POLLIN) printf(" POLLIN");
    if (f & POLLNVAL) printf(" POLLNVAL");
    if (f & POLLOUT) printf(" POLLOUT");
    if (f & POLLPRI) printf(" POLLPRI");
    if (f & POLLRDBAND) printf(" POLLRDBAND");
    if (f & POLLRDNORM) printf(" POLLRDNORM");
    if (f & POLLWRBAND) printf(" POLLWRBAND");
    if (f & POLLWRNORM) printf(" POLLWRNORM");
    printf("\n");
}

void evl_poll(struct event_loop_state *state, const struct timespec *timeout)
{
    int nfds = 0;
    int nfds_out;
    const int itim = timeout ? timeout->tv_sec + (int)(timeout->tv_nsec / 1000000000) : -1;

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

    /* TODO EINTR might affect the timeout. */
    while ((nfds_out = poll(state->pfds, nfds, itim)) == -1 &&
           (errno == EINTR || errno == EAGAIN)) {
        continue;
    }
    if (nfds_out > 0) {
        const int offset = state->nr_pending;
        int nr_events = 0;
        printf("Hello fds\n");

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
