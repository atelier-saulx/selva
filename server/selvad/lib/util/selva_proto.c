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

const char *selva_proto_type_to_str(enum selva_proto_data_type type)
{
    switch (type) {
    case SELVA_PROTO_NULL:
        return "null";
    case SELVA_PROTO_ERROR:
        return "error";
    case SELVA_PROTO_DOUBLE:
        return "double";
    case SELVA_PROTO_LONGLONG:
        return "longlong";
    case SELVA_PROTO_STRING:
        return "string";
    case SELVA_PROTO_ARRAY:
        return "array";
    case SELVA_PROTO_ARRAY_END:
        return "array end";
    }

    return "invalid";
}

static int parse_hdr_null(const char *buf __unused, size_t bsize __unused, enum selva_proto_data_type *type_out, size_t *len_out)
{
    *type_out = SELVA_PROTO_NULL;
    *len_out = 0;

    return sizeof(struct selva_proto_null);
}

static int parse_hdr_error(const char *buf, size_t bsize, enum selva_proto_data_type *type_out, size_t *len_out)
{
    struct selva_proto_error hdr;

    if (bsize < sizeof(hdr)) {
        return SELVA_PROTO_EBADMSG;
    };

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

static int parse_hdr_string(const char *buf, size_t bsize, enum selva_proto_data_type *type_out, size_t *len_out)
{
    struct selva_proto_string hdr;

    if (bsize < sizeof(hdr)) {
        return SELVA_PROTO_EBADMSG;
    }

    memcpy(&hdr, buf, sizeof(hdr));
    hdr.bsize = le16toh(hdr.bsize);

    *type_out = SELVA_PROTO_STRING;
    *len_out = hdr.bsize;

    return sizeof(hdr) + hdr.bsize;
}

static int parse_hdr_array(const char *buf, size_t bsize, enum selva_proto_data_type *type_out, size_t *len_out)
{
    struct selva_proto_array hdr;

    if (bsize < sizeof(hdr)) {
        return SELVA_PROTO_EBADMSG;
    }

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

int selva_proto_parse_vtype(const void *buf, size_t bsize, size_t i, enum selva_proto_data_type *type_out, size_t *len_out)
{
    size_t val_size = bsize - i; /* max size guess */
    struct selva_proto_control ctrl;

    if (i >= bsize) {
        return SELVA_PROTO_EINVAL;
    }

    if (val_size < sizeof(ctrl)) {
        return SELVA_PROTO_EBADMSG;
    }

    memcpy(&ctrl, buf + i, sizeof(ctrl));
    if ((unsigned)ctrl.type <= num_elem(parse_hdr)) {
        return parse_hdr[ctrl.type]((char *)buf + i, val_size, type_out, len_out);
    }

    return SELVA_PROTO_EBADMSG;
}

int selva_proto_parse_error(const void *buf, size_t bsize, size_t i, int *err_out, const char **msg_str_out, size_t *msg_len_out)
{
    size_t val_size = bsize - i;
    struct selva_proto_error hdr;

    if (val_size < sizeof(hdr)) {
        return SELVA_PROTO_EBADMSG;
    }

    memcpy(&hdr, (char *)buf + i, sizeof(hdr));
    if (hdr.type != SELVA_PROTO_ERROR) {
        return SELVA_PROTO_EBADMSG;
    }

    hdr.err_code = le16toh(hdr.err_code);
    hdr.bsize = le16toh(hdr.bsize);

    if (err_out) {
        *err_out = hdr.err_code;
    }
    if (msg_str_out && msg_len_out) {
        if (hdr.bsize == 0 || hdr.bsize > (val_size - sizeof(hdr))) {
            *msg_str_out = NULL;
            *msg_len_out = 0;
        } else {
            *msg_str_out = (char *)buf + i + sizeof(hdr);
            *msg_len_out = hdr.bsize;
        }
    }
    return 0;
}
