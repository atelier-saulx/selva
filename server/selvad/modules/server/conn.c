/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "util/bitmap.h"
#include "selva_proto.h"
#include "server.h"
#include "../../tunables.h"

/**
 * Client conn_ctx allocation map.
 * 0 = in use;
 * 1 = free.
 */
static struct bitmap clients_map = BITMAP_INIT(SERVER_MAX_CLIENTS);
static struct conn_ctx clients[SERVER_MAX_CLIENTS];

struct conn_ctx *alloc_conn_ctx(void)
{
    int i;
    struct conn_ctx *ctx = NULL;

    i = bitmap_ffs(&clients_map);
    if (i >= 0) {
        bitmap_clear(&clients_map, i);
        ctx = &clients[i];
        ctx->inuse = i;
    }

    return ctx;
}

void free_conn_ctx(struct conn_ctx *ctx)
{
    int i = ctx->inuse;

    ctx->inuse = 0;
    bitmap_set(&clients_map, i);
}

size_t conn_to_str(struct conn_ctx *ctx, char buf[CONN_STR_LEN], size_t bsize)
{
    struct sockaddr_in addr; /*!< Client/peer addr */
    socklen_t addr_size = sizeof(struct sockaddr_in);

    memset(buf, '\0', bsize); /* bc inet_ntop() may not terminate. */
    if (unlikely(bsize < CONN_STR_LEN)) {
        return 0;
    }

    if (getpeername(ctx->fd, (struct sockaddr *)&addr, &addr_size) == -1) {
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

__constructor void init_conn(void)
{
    for (size_t i = 0; i < SERVER_MAX_CLIENTS; i++) {
        bitmap_set(&clients_map, i);
    }
}
