/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

void tcp_set_nodelay(int fd);
void tcp_unset_nodelay(int fd);
void tcp_set_keepalive(int fd, int time, int intvl, int probes);
void tcp_cork(int fd);
void tcp_uncork(int fd);
ssize_t tcp_recv(int fd, void *buf, size_t n, int flags);
ssize_t tcp_read(int fd, void *buf, size_t n);

/* TODO fix fuzzer with tcp_read() */

#ifndef FUZZER
#define tcp_write write
#define tcp_send send
#define tcp_sendto sendto
#define tcp_sendmsg sendmsg
#endif
