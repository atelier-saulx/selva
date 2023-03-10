/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "cdefs.h"
#include "endian.h"
#include "util/crc32c.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_proto.h"

int selva_proto_verify_frame_chk(
        struct selva_proto_header * restrict hdr,
        const void * restrict payload,
        size_t size)
{
    uint32_t orig_chk;
    uint32_t comp_chk;

    orig_chk = le32toh(hdr->chk);
    hdr->chk = 0;
    comp_chk = crc32c(0, hdr, sizeof(*hdr));
    hdr->chk = orig_chk;

    if (size > 0) {
        comp_chk = crc32c(comp_chk, payload, size);
    }

    return comp_chk == orig_chk;
}

const char *selva_proto_type_to_str(enum selva_proto_data_type type, size_t *len)
{
    size_t tmp;

    if (!len) {
        len = &tmp;
    }

    switch (type) {
    case SELVA_PROTO_NULL:
        *len = 4;
        return "null";
    case SELVA_PROTO_ERROR:
        *len = 5;
        return "error";
    case SELVA_PROTO_DOUBLE:
        *len = 6;
        return "double";
    case SELVA_PROTO_LONGLONG:
        *len = 8;
        return "longlong";
    case SELVA_PROTO_STRING:
        *len = 6;
        return "string";
    case SELVA_PROTO_ARRAY:
        *len = 5;
        return "array";
    case SELVA_PROTO_ARRAY_END:
        *len = 9;
        return "array end";
    case SELVA_PROTO_REPLICATION_CMD:
        *len = 15;
        return "replication cmd";
    case SELVA_PROTO_REPLICATION_SDB:
        *len = 15;
        return "replication sdb";
    }

    *len = 7;
    return "invalid";
}

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

static int parse_hdr_replication_cmd(const char *buf __unused, size_t bsize __unused, enum selva_proto_data_type *type_out, size_t *len_out)
{
    struct selva_proto_replication_cmd hdr;

    memcpy(&hdr, buf, sizeof(hdr));
    hdr.bsize = le64toh(hdr.bsize);

    *type_out = SELVA_PROTO_REPLICATION_CMD;
    *len_out = hdr.bsize;
    return sizeof(hdr) + hdr.bsize;
}

static int parse_hdr_replication_sdb(const char *buf __unused, size_t bsize __unused, enum selva_proto_data_type *type_out, size_t *len_out)
{
    struct selva_proto_replication_sdb hdr;

    memcpy(&hdr, buf, sizeof(hdr));
    hdr.bsize = le64toh(hdr.bsize);

    *type_out = SELVA_PROTO_REPLICATION_SDB;
    *len_out = hdr.bsize;
    return sizeof(hdr) + hdr.bsize;
}

static struct {
    int (*const fn)(const char *buf, size_t bsize, enum selva_proto_data_type *type_out, size_t *len_out);
    size_t hdr_size;
} parse_hdr[] = {
    { parse_hdr_null, sizeof(struct selva_proto_null) },
    { parse_hdr_error, sizeof(struct selva_proto_error) },
    { parse_hdr_double, sizeof(struct selva_proto_double) },
    { parse_hdr_longlong, sizeof(struct selva_proto_longlong) },
    { parse_hdr_string, sizeof(struct selva_proto_string) },
    { parse_hdr_array, sizeof(struct selva_proto_array) },
    { parse_hdr_array_end, sizeof(struct selva_proto_control) },
    { parse_hdr_replication_cmd, sizeof(struct selva_proto_replication_cmd) },
    { parse_hdr_replication_sdb, sizeof(struct selva_proto_replication_sdb) },
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
        if (val_size >= parse_hdr[ctrl.type].hdr_size) {
            return parse_hdr[ctrl.type].fn((char *)buf + i, val_size, type_out, len_out);
        }
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

int selva_proto_parse_replication_cmd(const void *buf, size_t bsize, size_t i, uint64_t *eid, int8_t *cmd_id, size_t *data_size)
{
    size_t val_size = bsize - i;
    struct selva_proto_replication_cmd hdr;

    if (val_size < sizeof(hdr)) {
        return SELVA_PROTO_EBADMSG;
    }

    memcpy(&hdr, (char *)buf + i, sizeof(hdr));
    if (hdr.type != SELVA_PROTO_REPLICATION_CMD) {
        return SELVA_PROTO_EBADMSG;
    }

    *eid = le64toh(hdr.eid);
    *cmd_id = hdr.cmd;
    *data_size = le64toh(hdr.bsize);
    return 0;
}

int selva_proto_parse_replication_sdb(const void *buf, size_t bsize, size_t i, uint64_t *eid, size_t *data_size)
{
    size_t val_size = bsize - i;
    struct selva_proto_replication_sdb hdr;

    if (val_size < sizeof(hdr)) {
        return SELVA_PROTO_EBADMSG;
    }

    memcpy(&hdr, (char *)buf + i, sizeof(hdr));
    if (hdr.type != SELVA_PROTO_REPLICATION_SDB) {
        return SELVA_PROTO_EBADMSG;
    }

    *eid = le64toh(hdr.eid);
    *data_size = le64toh(hdr.bsize);
    return 0;
}
