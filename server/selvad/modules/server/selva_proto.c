/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <alloca.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include "selva_error.h"
#include "selva_proto.h"
#include "server.h"

/* TODO Return proper errors. */

int selva_proto_send_error(struct conn_ctx *ctx, int err, const char *msg_str, size_t msg_len)
{
    const size_t bsize = sizeof(struct selva_proto_error) + msg_len;
    struct selva_proto_error *buf = alloca(bsize);

    *buf = (struct selva_proto_error){
        .type = SELVA_PROTO_ERROR,
        .err_code = err,
        .bsize = msg_len,
    };
    memcpy(buf->msg, msg_str, msg_len);

    send(ctx->fd, buf, bsize, 0);
    return 0;
}

int selva_proto_send_double(struct conn_ctx *ctx, double value)
{
    struct selva_proto_double buf = {
        .type = SELVA_PROTO_DOUBLE,
        .v = value,
    };

    send(ctx->fd, &buf, sizeof(buf), 0);
    return 0;
}

int selva_proto_send_ll(struct conn_ctx *ctx, long long value)
{
    struct selva_proto_double buf = {
        .type = SELVA_PROTO_LONGLONG,
        .v = value,
    };

    send(ctx->fd, &buf, sizeof(buf), 0);
    return 0;
}

int selva_proto_send_str(struct conn_ctx *ctx, const char *str, size_t len)
{
    const size_t bsize = sizeof(struct selva_proto_error) + len;
    struct selva_proto_error *buf = alloca(bsize);

    *buf = (struct selva_proto_error){
        .type = SELVA_PROTO_STRING,
        .bsize = len,
    };
    memcpy(buf->msg, str, len);

    send(ctx->fd, &buf, bsize, 0);
    return 0;
}

/**
 * If `len` is set negative then selva_proto_send_array_end() should be used to
 * terminate the array.
 * @param len Number if items in the array.
 */
int selva_proto_send_array(struct conn_ctx *ctx, int len)
{
    struct selva_proto_array buf = {
        .type = SELVA_PROTO_ARRAY,
    };

    if (len >= 0) {
        buf.length = len;
    } else {
        buf.flags = SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH;
    }

    send(ctx->fd, &buf, sizeof(buf), 0);
    return 0;
}

int selva_proto_send_array_end(struct conn_ctx *ctx)
{
    struct selva_proto_control buf = {
        .type = SELVA_PROTO_ARRAY_END,
    };

    send(ctx->fd, &buf, sizeof(buf), 0);
    return 0;
}
