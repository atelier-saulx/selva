/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "cdefs.h"
#include "endian.h"
#include "selva_error.h"
#include "selva_proto.h"
#include "util/selva_string.h"

/* FIXME check bsize before copying from buf */

static int parse_hdr_null(const char *buf __unused, size_t bsize __unused, enum selva_proto_data_type *type_out, size_t *len_out)
{
    *type_out = SELVA_PROTO_NULL;
    *len_out = 0;

    return sizeof(struct selva_proto_null);
}

static int parse_hdr_error(const char *buf, size_t bsize __unused, enum selva_proto_data_type *type_out, size_t *len_out)
{
    struct selva_proto_error hdr;

    memcpy(&hdr, buf, sizeof(hdr));
    hdr.bsize = le16toh(hdr.bsize);

    *type_out = SELVA_PROTO_ERROR;
    *len_out = hdr.bsize;

    return sizeof(hdr) + hdr.bsize;
}

static int parse_hdr_double(const char *buf __unused, size_t bsize __unused, enum selva_proto_data_type *type_out, size_t *len_out)
{
    *type_out = SELVA_PROTO_DOUBLE;
    *len_out = sizeof(double);

    return sizeof(struct selva_proto_double);
}

static int parse_hdr_longlong(const char *buf __unused, size_t bsize __unused, enum selva_proto_data_type *type_out, size_t *len_out)
{
    *type_out = SELVA_PROTO_LONGLONG;
    *len_out = sizeof(long long);

    return sizeof (struct selva_proto_longlong);
}

static int parse_hdr_string(const char *buf, size_t bsize __unused, enum selva_proto_data_type *type_out, size_t *len_out)
{
    struct selva_proto_string hdr;

    memcpy(&hdr, buf, sizeof(hdr));
    hdr.bsize = le16toh(hdr.bsize);

    *type_out = SELVA_PROTO_STRING;
    *len_out = hdr.bsize;

    return sizeof(hdr) + hdr.bsize;
}

static int parse_hdr_array(const char *buf, size_t bsize __unused, enum selva_proto_data_type *type_out, size_t *len_out)
{
    struct selva_proto_array hdr;

    memcpy(&hdr, buf, sizeof(hdr));
    hdr.length = le32toh(hdr.length);

    *type_out = SELVA_PROTO_ARRAY;
    *len_out = hdr.length;
    return sizeof(hdr);
}

static int parse_hdr_array_end(const char *buf __unused, size_t bsize __unused, enum selva_proto_data_type *type_out, size_t *len_out)
{
    *type_out = SELVA_PROTO_ARRAY_END;
    *len_out = 0;

    return sizeof(struct selva_proto_control);
}

static int (*const parse_hdr[])(const char *buf, size_t bsize, enum selva_proto_data_type *type_out, size_t *len_out) = {
    parse_hdr_null,
    parse_hdr_error,
    parse_hdr_double,
    parse_hdr_longlong,
    parse_hdr_string,
    parse_hdr_array,
    parse_hdr_array_end,
};

int selva_proto_parse_vtype(const char *buf, size_t bsize, size_t i, enum selva_proto_data_type *type_out, size_t *len_out)
{
    size_t msg_size = bsize - i; /* max guess */
    struct selva_proto_control ctrl;

    if (msg_size < sizeof(struct selva_proto_control)) {
        return SELVA_PROTO_EBADMSG;
    }

    memcpy(&ctrl, buf + i, sizeof(ctrl));
    if (ctrl.type >= 0 && ctrl.type <= SELVA_PROTO_ARRAY_END) {
        return parse_hdr[ctrl.type](buf + i, msg_size, type_out, len_out);
    }

    return SELVA_PROTO_EBADMSG;
}
