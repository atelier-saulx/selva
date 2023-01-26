/*
 * High level send functions.
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <alloca.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "endian.h"
#include "selva_error.h"
#include "selva_proto.h"
#include "util/selva_string.h"
#include "selva_server.h"
#include "server.h"

int selva_send_flush(struct selva_server_response_out *restrict resp)
{
    if (!resp->ctx) {
        return SELVA_PROTO_ENOTCONN;
    }

    return server_flush_frame_buf(resp, 0);
}

int selva_send_null(struct selva_server_response_out *resp)
{
    struct selva_proto_null buf = {
        .type = SELVA_PROTO_NULL,
    };

    return selva_send_buf(resp, &buf, sizeof(buf));
}

int selva_send_error(struct selva_server_response_out *resp, int err, const char *msg_str, size_t msg_len)
{
    size_t bsize;
    struct selva_proto_error *buf;

    if (!msg_str) {
        msg_len = 0;
    }

    bsize = sizeof(struct selva_proto_error) + msg_len;
    buf = alloca(bsize);
    *buf = (struct selva_proto_error){
        .type = SELVA_PROTO_ERROR,
        .err_code = htole16(err),
        .bsize = htole16(msg_len),
    };

    if (msg_len > 0) {
        memcpy(buf->msg, msg_str, msg_len);
    }

    return selva_send_buf(resp, buf, bsize);
}

int selva_send_errorf(struct selva_server_response_out *resp, int err, const char *fmt, ...)
{
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0) {
        return SELVA_EINVAL;
    }

    size_t bsize = sizeof(struct selva_proto_error) + len;
    struct selva_proto_error *buf = alloca(bsize + 1);
    *buf = (struct selva_proto_error){
        .type = SELVA_PROTO_ERROR,
        .err_code = htole16(err),
        .bsize = htole16(len),
    };

    va_start(args, fmt);
    (void)vsnprintf(buf->msg, len + 1, fmt, args);
    va_end(args);

    return selva_send_buf(resp, buf, bsize);
}

int selva_send_error_arity(struct selva_server_response_out *resp)
{
    return selva_send_error(resp, SELVA_EINVAL, "Wrong arity", 11);
}

int selva_send_double(struct selva_server_response_out *resp, double value)
{
    struct selva_proto_double buf = {
        .type = SELVA_PROTO_DOUBLE,
    };

    htoledouble((char *)&buf.v, value);

    return selva_send_buf(resp, &buf, sizeof(buf));
}

int selva_send_ll(struct selva_server_response_out *resp, long long value)
{
    struct selva_proto_longlong buf = {
        .type = SELVA_PROTO_LONGLONG,
        .v = htole64(value),
    };

    return selva_send_buf(resp, &buf, sizeof(buf));
}

int selva_send_str(struct selva_server_response_out *resp, const char *str, size_t len)
{
    const size_t bsize = sizeof(struct selva_proto_string) + len;
    struct selva_proto_string *buf = alloca(bsize);

    *buf = (struct selva_proto_string){
        .type = SELVA_PROTO_STRING,
        .bsize = htole32(len),
    };
    memcpy(buf->data, str, len);

    return selva_send_buf(resp, buf, bsize);
}

int selva_send_strf(struct selva_server_response_out *resp, const char *fmt, ...)
{
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0) {
        return SELVA_EINVAL;
    }

    const size_t bsize = sizeof(struct selva_proto_string) + len;
    struct selva_proto_string *buf = alloca(bsize + 1);

    *buf = (struct selva_proto_string){
        .type = SELVA_PROTO_STRING,
        .bsize = htole32(len),
    };

    va_start(args, fmt);
    (void)vsnprintf(buf->data, len + 1, fmt, args);
    va_end(args);

    return selva_send_buf(resp, buf, bsize);
}

int selva_send_string(struct selva_server_response_out *resp, const struct selva_string *s)
{
    TO_STR(s);

    return selva_send_str(resp, s_str, s_len);
}

int selva_send_bin(struct selva_server_response_out *resp, const void *b, size_t len)
{
    const size_t bsize = sizeof(struct selva_proto_string) + len;
    struct selva_proto_string *buf = alloca(bsize);

    *buf = (struct selva_proto_string){
        .type = SELVA_PROTO_STRING,
        .flags = SELVA_PROTO_STRING_FBINARY,
        .bsize = htole32(len),
    };
    memcpy(buf->data, b, len);

    return selva_send_buf(resp, buf, bsize);
}

int selva_send_array(struct selva_server_response_out *resp, int len)
{
    struct selva_proto_array buf = {
        .type = SELVA_PROTO_ARRAY,
    };

    if (len >= 0) {
        buf.length = htole32(len);
    } else {
        buf.flags = SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH;
    }

    return selva_send_buf(resp, &buf, sizeof(buf));
}

int selva_send_array_end(struct selva_server_response_out *resp)
{
    struct selva_proto_control buf = {
        .type = SELVA_PROTO_ARRAY_END,
    };

    return selva_send_buf(resp, &buf, sizeof(buf));
}
