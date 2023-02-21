/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "cdefs.h"
#include "util/net.h"

size_t fd_to_str(int fd, char buf[CONN_STR_LEN], size_t bsize)
{
    struct sockaddr_in addr; /*!< Client/peer addr */
    socklen_t addr_size = sizeof(struct sockaddr_in);

    memset(buf, '\0', bsize); /* bc inet_ntop() may not terminate. */
    if (unlikely(bsize < CONN_STR_LEN)) {
        return 0;
    }

    if (getpeername(fd, (struct sockaddr *)&addr, &addr_size) == -1) {
        const int e = errno;

        static_assert(CONN_STR_LEN > 17);

        switch (e) {
        case ENOBUFS:
            strcpy(buf, "<sys error>");
            return 11;
        case EBADF:
        case ENOTCONN:
        case ENOTSOCK:
            strcpy(buf, "<not connected>");
            return 15;
        case EFAULT:
        case EINVAL:
        default:
            strcpy(buf, "<internal error>");
            return 16;
        }
    }

    if (!inet_ntop(AF_INET, &addr.sin_addr, buf, bsize)) {
        strcpy(buf, "<ntop failed>");
        return 13;
    }

    const ssize_t end = strlen(buf);
    const int n = bsize - end;
    const int res = snprintf(buf + end, n, ":%d", ntohs(addr.sin_port));

    return (res > 0 && res < n) ? end + n : end;
}
