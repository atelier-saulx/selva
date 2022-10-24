#pragma once

struct event_loop_state;
struct timespec;

void evl_poll_init(struct event_loop_state *state);
void evl_poll_deinit(struct event_loop_state *state);
int evl_poll_add_fd(struct event_loop_state *state, int fd, enum event_type mask);
void evl_poll_del_fd(struct event_loop_state *state, int fd, enum event_type mask);
void evl_poll(struct event_loop_state *state, const struct timespec *timeout);
