/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "tcp.h"

static const int use_tcp_nodelay = 1;

void tcp_set_nodelay(int fd)
{
    if (use_tcp_nodelay) {
        (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));
    }
}

void tcp_unset_nodelay(int fd)
{
    if (use_tcp_nodelay) {
        (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &(int){0}, sizeof(int));
    }
}

void tcp_cork(int fd)
{
#if __linux__
    (void)setsockopt(fd, IPPROTO_TCP, TCP_CORK, &(int){1}, sizeof(int));
#else
    tcp_unset_nodelay(fd);
#endif
}

void tcp_uncork(int fd)
{
#if __linux__
    (void)setsockopt(fd, IPPROTO_TCP, TCP_CORK, &(int){0}, sizeof(int));
#else
    const char *buf = "";

    tcp_set_nodelay(fd);
    send(fd, buf, 0, 0);
#endif
}
