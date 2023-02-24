/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
/* FIXME Remove unused includes. */
#include <arpa/inet.h>
#include <assert.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include "util/bitmap.h"
#include "util/net.h"
#include "event_loop.h"
#include "selva_proto.h"
#include "selva_log.h"
#include "server.h"
#include "../../tunables.h"

#define ALL_STREAMS_FREE ((1 << MAX_STREAMS) - 1)

#define STREAM_WRITER_RETRY_SEC 15

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
        memset(ctx, 0, sizeof(*ctx));
        atomic_init(&ctx->streams.free_map, ALL_STREAMS_FREE);
        ctx->inuse = i;
        ctx->corked = 0;
    }

    return ctx;
}


static void retry_free_con_ctx(struct event *, void *arg)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Retrying free_conn_ctx(%p)\n", arg);
    free_conn_ctx((struct conn_ctx *)arg);
}

void free_conn_ctx(struct conn_ctx *ctx)
{
    const unsigned free_map = atomic_load(&ctx->streams.free_map);

    if (free_map == ALL_STREAMS_FREE) {
        int i = ctx->inuse;

        close(ctx->fd);
        ctx->inuse = 0;
        bitmap_set(&clients_map, i);
    } else {
        /* Wait for stream writers to terminate. */
        const struct timespec t = {
            .tv_sec = STREAM_WRITER_RETRY_SEC,
            .tv_nsec = 0,
        };

        SELVA_LOG(SELVA_LOGL_INFO, "conn_ctx (%p) %d stream(s) busy, waiting...\n",
                  ctx,
                  MAX_STREAMS - __builtin_popcount(free_map));

        (void)evl_set_timeout(&t, retry_free_con_ctx, ctx);
    }
}

struct selva_server_response_out *alloc_stream_resp(struct conn_ctx *ctx)
{
    unsigned old_free_map, new_free_map, i;

    do {
        old_free_map = atomic_load(&ctx->streams.free_map);
        i = __builtin_ffs(old_free_map);
        if (i-- == 0) {
            return NULL;
        }

        new_free_map = old_free_map & ~(1 << i);
    } while (!atomic_compare_exchange_weak(&ctx->streams.free_map, &old_free_map, new_free_map));

    return &ctx->streams.stream_resp[i];
}

void free_stream_resp(struct selva_server_response_out *stream_resp)
{
    struct conn_ctx *ctx;
    unsigned old_free_map, new_free_map;

    assert(stream_resp->ctx);
    ctx = stream_resp->ctx;

    do {
        old_free_map = atomic_load(&ctx->streams.free_map);
        new_free_map = old_free_map | 1 << (unsigned)((ptrdiff_t)stream_resp - (ptrdiff_t)ctx->streams.stream_resp);
    } while (!atomic_compare_exchange_weak(&ctx->streams.free_map, &old_free_map, new_free_map));
}

size_t conn_to_str(struct conn_ctx *ctx, char buf[CONN_STR_LEN], size_t bsize)
{
    return fd_to_str(ctx->fd, buf, bsize);
}

__constructor void init_conn(void)
{
    for (size_t i = 0; i < SERVER_MAX_CLIENTS; i++) {
        bitmap_set(&clients_map, i);
    }
}
