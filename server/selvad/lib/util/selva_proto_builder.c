/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "cdefs.h"
#include "jemalloc.h"
#include "selva_proto.h"
#include "util/selva_proto_builder.h"

void selva_proto_builder_init(struct selva_proto_builder_msg *msg)
{
    msg->bsize = 0;
    msg->nr_values = 0;
    msg->buf = NULL;
}

static void encapsulate_with_array(struct selva_proto_builder_msg * restrict msg)
{
    struct selva_proto_array arr = {
        .type = SELVA_PROTO_ARRAY,
        .flags = SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH,
    };

    msg->buf = selva_realloc(msg->buf, msg->bsize + sizeof(arr));
    memcpy(msg->buf + msg->bsize, &arr, sizeof(arr));
    msg->bsize += sizeof(arr);
}

static void selva_proto_builder_insert_header(struct selva_proto_builder_msg * restrict msg, const void * restrict hdr, size_t hsize)
{
    if (msg->nr_values++  == 1) {
        encapsulate_with_array(msg);
    }

    msg->buf = selva_realloc(msg->buf, msg->bsize + hsize);
    memcpy(msg->buf + msg->bsize, hdr, hsize);
    msg->bsize += hsize;
}

static void selva_proto_builder_insert_payload(struct selva_proto_builder_msg * restrict msg, const void * restrict payload, size_t psize)
{
    if (payload && psize > 0) {
        msg->buf = selva_realloc(msg->buf, msg->bsize + psize);
        memcpy(msg->buf + msg->bsize, payload, psize);
        msg->bsize += psize;
    }
}

void selva_proto_builder_end(struct selva_proto_builder_msg *msg)
{
    if (msg->nr_values > 1) {
        struct selva_proto_control ctrl = {
            .type = SELVA_PROTO_ARRAY_END,
        };

        msg->buf = selva_realloc(msg->buf, msg->bsize + sizeof(ctrl));
        memcpy(msg->buf + msg->bsize, &ctrl, sizeof(ctrl));
        msg->bsize += sizeof(ctrl);
    }
}

void selva_proto_builder_deinit(struct selva_proto_builder_msg *msg)
{
    selva_free(msg->buf);
}

void selva_proto_builder_insert_null(struct selva_proto_builder_msg *msg)
{
    struct selva_proto_null hdr = {
        .type = SELVA_PROTO_NULL,
    };

    selva_proto_builder_insert_header(msg, &hdr, sizeof(hdr));
}

void selva_proto_builder_insert_double(struct selva_proto_builder_msg *msg, double v)
{
    struct selva_proto_double hdr = {
        .type = SELVA_PROTO_DOUBLE,
        .v = v,
    };

    selva_proto_builder_insert_header(msg, &hdr, sizeof(hdr));
}

void selva_proto_builder_insert_longlong(struct selva_proto_builder_msg *msg, long long v)
{
    struct selva_proto_longlong hdr = {
        .type = SELVA_PROTO_LONGLONG,
        .v = v,
    };

    selva_proto_builder_insert_header(msg, &hdr, sizeof(hdr));
}

void selva_proto_builder_insert_string(struct selva_proto_builder_msg * restrict msg, const char * restrict str, size_t len)
{
    struct selva_proto_string hdr = {
        .type = SELVA_PROTO_STRING,
        .bsize = len,
    };

    selva_proto_builder_insert_header(msg, &hdr, sizeof(hdr));
    selva_proto_builder_insert_payload(msg, str, len);
}

void selva_proto_builder_insert_array(struct selva_proto_builder_msg *msg)
{
    struct selva_proto_array hdr = {
        .type = SELVA_PROTO_ARRAY,
        .flags = SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH,
    };

    selva_proto_builder_insert_header(msg, &hdr, sizeof(hdr));
}

void selva_proto_builder_insert_array_end(struct selva_proto_builder_msg *msg)
{
    struct selva_proto_control hdr = {
        .type = SELVA_PROTO_ARRAY_END,
    };

    selva_proto_builder_insert_header(msg, &hdr, sizeof(hdr));
}
