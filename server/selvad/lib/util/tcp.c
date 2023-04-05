/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#if __linux__
#include <arpa/inet.h> /* Linux defines IPPROTO_TCP in here */
#include <sys/sendfile.h>
#endif
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#if __APPLE__ /* sendfile on MacOs */
#include <sys/types.h>
#include <sys/uio.h>
#endif
#include <unistd.h>
#include "util/tcp.h"

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

void tcp_set_keepalive(int fd, int time, int intvl, int probes)
{
#if __linux__
    (void)setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &(int){1}, sizeof(int));
    (void)setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &time, sizeof(time));
    (void)setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    (void)setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &probes, sizeof(probes));
#else
    (void)fd;
    (void)time;
    (void)intvl;
    (void)probes;
#endif
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

ssize_t tcp_recv(int fd, void *buf, size_t n, int flags)
{
	ssize_t i = 0;

	while (i < (ssize_t)n) {
		ssize_t res;

		res = recv(fd, (char *)buf + i, n - i, flags);
		if (res <= 0) {
			return i;
		}

		i += res;
	}

	return i;
}

ssize_t tcp_read(int fd, void *buf, size_t n)
{
	ssize_t i = 0;

	while (i < (ssize_t)n) {
		ssize_t res;

		res = read(fd, (char *)buf + i, n - i);
		if (res <= 0) {
			return i;
		}

		i += res;
	}

	return i;
}

off_t tcp_sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
    off_t bytes_sent;

#if __APPLE__
    int err;

    bytes_sent = count;
    err = sendfile(in_fd, out_fd, *offset, &bytes_sent, NULL, 0);
    if (err) {
        bytes_sent = -1;
    }
#elif __linux__
    bytes_sent = sendfile(out_fd, in_fd, offset, count);
#else
#error "Not implemented"
#endif

    return bytes_sent;
}
