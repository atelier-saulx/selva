/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <alloca.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "endian.h"
#include "selva_error.h"
#include "selva_proto.h"
#define SELVA_SERVER_MAIN 1
#include "selva_server.h"
#include "server.h"

int selva_send_error(struct selva_server_response_out *resp, int err, const char *msg_str, size_t msg_len)
{
    const size_t bsize = sizeof(struct selva_proto_error) + msg_len;
    struct selva_proto_error *buf = alloca(bsize);

    *buf = (struct selva_proto_error){
        .type = SELVA_PROTO_ERROR,
        .err_code = htole16(err),
        .bsize = htole16(msg_len),
    };
    memcpy(buf->msg, msg_str, msg_len);

    return server_send_buf(resp, buf, bsize);
}

int selva_send_double(struct selva_server_response_out *resp, double value)
{
    struct selva_proto_double buf = {
        .type = SELVA_PROTO_DOUBLE,
    };

    htoledouble((char *)&buf.v, value);

    return server_send_buf(resp, &buf, sizeof(buf));
}

int selva_send_ll(struct selva_server_response_out *resp, long long value)
{
    struct selva_proto_double buf = {
        .type = SELVA_PROTO_LONGLONG,
        .v = htole64(value),
    };

    return server_send_buf(resp, &buf, sizeof(buf));
}

int selva_send_str(struct selva_server_response_out *resp, const char *str, size_t len)
{
    const size_t bsize = sizeof(struct selva_proto_string) + len;
    struct selva_proto_string *buf = alloca(bsize);

    *buf = (struct selva_proto_string){
        .type = SELVA_PROTO_STRING,
        .bsize = htole32(len),
    };
    memcpy(buf->str, str, len);

    return server_send_buf(resp, buf, bsize);
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

    return server_send_buf(resp, &buf, sizeof(buf));
}

int selva_send_array_end(struct selva_server_response_out *resp)
{
    struct selva_proto_control buf = {
        .type = SELVA_PROTO_ARRAY_END,
    };

    return server_send_buf(resp, &buf, sizeof(buf));
}
