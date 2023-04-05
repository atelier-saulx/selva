/*
 * High level send functions.
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "endian.h"
#include "selva_error.h"
#include "selva_proto.h"
#include "util/selva_string.h"
#include "selva_server.h"
#include "server.h"

#ifndef O_NOATIME
#define O_NOATIME 00
#endif

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
    ssize_t res;

    res = server_send_buf(resp, &buf, sizeof(buf), 0);
    return (res < 0) ? (int)res : 0;
}

int selva_send_error(struct selva_server_response_out *resp, int err, const char *msg_str, size_t msg_len)
{
    msg_len = msg_str ? msg_len : 0;
    const struct selva_proto_error hdr = {
        .type = SELVA_PROTO_ERROR,
        .err_code = htole16(err),
        .bsize = htole16(msg_len),
    };
    int res;

    resp->last_error = err;

    res = server_send_buf(resp, &hdr, sizeof(hdr), SERVER_SEND_MORE);
    if (res == sizeof(hdr) && msg_len > 0) {
        res = server_send_buf(resp, msg_str, msg_len, 0);
    }

    return (res < 0) ? (int)res : 0;
}

int selva_send_errorf(struct selva_server_response_out *resp, int err, const char *fmt, ...)
{
    va_list args;
    int len;
    ssize_t res;

    resp->last_error = err;

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

    res = server_send_buf(resp, buf, bsize, 0);
    return (res < 0) ? (int)res : 0;
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
    ssize_t res;

    htoledouble((char *)&buf.v, value);

    res = server_send_buf(resp, &buf, sizeof(buf), 0);
    return (res < 0) ? (int)res : 0;
}

int selva_send_ll(struct selva_server_response_out *resp, long long value)
{
    struct selva_proto_longlong buf = {
        .type = SELVA_PROTO_LONGLONG,
        .v = htole64(value),
    };
    ssize_t res;

    res = server_send_buf(resp, &buf, sizeof(buf), 0);
    return (res < 0) ? (int)res : 0;
}

int selva_send_llx(struct selva_server_response_out *resp, long long value)
{
    struct selva_proto_longlong buf = {
        .type = SELVA_PROTO_LONGLONG,
        .flags = SELVA_PROTO_LONGLONG_FMT_HEX,
        .v = htole64(value),
    };
    ssize_t res;

    res = server_send_buf(resp, &buf, sizeof(buf), 0);
    return (res < 0) ? (int)res : 0;
}

static int selva_send_str_wflags(struct selva_server_response_out *resp, const char *str, size_t len, typeof_field(struct selva_proto_string, flags) flags)
{
    struct selva_proto_string hdr = {
        .type = SELVA_PROTO_STRING,
        .flags = flags,
        .bsize = htole32(len),
    };
    ssize_t res;

    res = server_send_buf(resp, &hdr, sizeof(hdr), SERVER_SEND_MORE);
    if (res == sizeof(hdr)) {
        res = server_send_buf(resp, str, len, 0);
    }

    return (res < 0) ? (int)res : 0;
}

int selva_send_str(struct selva_server_response_out *resp, const char *str, size_t len)
{
    return selva_send_str_wflags(resp, str, len, 0);
}

int selva_send_strf(struct selva_server_response_out *resp, const char *fmt, ...)
{
    va_list args;
    int len;
    ssize_t res;

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

    res = server_send_buf(resp, buf, bsize, 0);
    return (res < 0) ? (int)res : 0;
}

int selva_send_string(struct selva_server_response_out *resp, const struct selva_string *s)
{
    TO_STR(s);
    typeof_field(struct selva_proto_string, flags) flags = (selva_string_get_flags(s) & SELVA_STRING_COMPRESS) ? SElVA_PROTO_STRING_FDEFLATE : 0;

    return selva_send_str_wflags(resp, s_str, s_len, flags);
}

int selva_send_bin(struct selva_server_response_out *resp, const void *b, size_t len)
{
    return selva_send_str_wflags(resp, b, len, SELVA_PROTO_STRING_FBINARY);
}

int selva_send_array(struct selva_server_response_out *resp, int len)
{
    struct selva_proto_array buf = {
        .type = SELVA_PROTO_ARRAY,
    };
    ssize_t res;

    if (len >= 0) {
        buf.length = htole32(len);
    } else {
        buf.flags = SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH;
    }

    res = server_send_buf(resp, &buf, sizeof(buf), 0);
    return (res < 0) ? (int)res : 0;
}

int selva_send_array_end(struct selva_server_response_out *resp)
{
    struct selva_proto_control buf = {
        .type = SELVA_PROTO_ARRAY_END,
    };
    ssize_t res;

    res = server_send_buf(resp, &buf, sizeof(buf), 0);
    return (res < 0) ? (int)res : 0;
}

int selva_send_replication_cmd(struct selva_server_response_out *resp, uint64_t eid, int64_t ts, int8_t cmd, const void *data, size_t bsize)
{
    struct selva_proto_replication_cmd buf = {
        .type = SELVA_PROTO_REPLICATION_CMD,
        .cmd = cmd,
        .eid = htole64(eid),
        .ts = htole64(ts),
        .bsize = htole64(bsize),
    };
    ssize_t res;

    res = server_send_buf(resp, &buf, sizeof(buf), SERVER_SEND_MORE);
    if (res < 0) {
        return (int)res;
    }
    res = server_send_buf(resp, data, bsize, 0);
    return (res < 0) ? (int)res : 0;
}

int selva_send_replication_sdb(struct selva_server_response_out *resp, uint64_t eid, const char *filename)
{
    int oflags = O_RDONLY | O_NOATIME;
    int fd;
    struct selva_proto_replication_sdb buf = {
        .type = SELVA_PROTO_REPLICATION_SDB,
        .flags = 0,
        .eid = htole64(eid),
    };
    ssize_t res;

    for (int retries = 3; retries; retries--) {
        fd = open(filename, oflags);
        if (fd != -1) {
            break;
        }

        switch (errno) {
        case EINTR:
            continue;
        case EPERM:
            oflags ^= O_NOATIME;
            continue;
        case EACCES:
        case EINVAL:
        case EISDIR:
        case ELOOP:
        case ENAMETOOLONG:
        case ENODEV:
        case ENOENT:
        case ENXIO:
        case EROFS:
            return SELVA_EINVAL;
        case EFBIG:
        case EMFILE:
        case ENFILE:
        case ENOMEM:
        case ENOSPC:
        case EOVERFLOW:
            return SELVA_ENOMEM;
        default:
            return SELVA_EGENERAL;
        }
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    buf.bsize = htole64((uint64_t)file_size);
    lseek(fd, 0, SEEK_SET);

    res = server_send_buf(resp, &buf, sizeof(buf), SERVER_SEND_MORE);
    if (res < 0) {
        close(fd);
        return (int)res;
    }

    res = server_send_file(resp, fd, file_size, 0);
    close(fd);

    return (res < 0) ? (int)res : 0;
}

int selva_send_replication_pseudo_sdb(struct selva_server_response_out *resp, uint64_t eid)
{
    struct selva_proto_replication_sdb buf = {
        .type = SELVA_PROTO_REPLICATION_SDB,
        .flags = 0,
        .eid = htole64(eid),
        .bsize = 0,
    };
    int res;

    res = server_send_buf(resp, &buf, sizeof(buf), 0);
    return (res < 0) ? (int)res : 0;
}

int selva_send_end(struct selva_server_response_out *restrict resp)
{
    int err;

    if (!resp->ctx) {
        return SELVA_PROTO_ENOTCONN;
    }

    err = server_flush_frame_buf(resp, 1);
    server_uncork_resp(resp);

    if (resp->frame_flags & SELVA_PROTO_HDR_STREAM) {
        /* Note that this function still needs resp->ctx. */
        free_stream_resp(resp);
    }

    resp->ctx = NULL; /* Make sure nothing will be sent anymore. */

    return err;
}
