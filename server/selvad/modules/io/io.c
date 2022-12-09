/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "endian.h"
#include "jemalloc.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_proto.h"
#include "selva_log.h"
#include "module.h"
#include "selva_io.h"
#include "sdb.h"

/* FIXME Implement write error handling */

#define READ_WHENCE_READ_HEADER "read header"
#define READ_WHENCE_HEADER_TYPE "type"
#define READ_WHENCE_VALUE       "read value"

/**
 * Test that the IO mode is set as expected.
 */
static void test_io_mode(struct selva_io *io, enum selva_io_flags mode)
{
    enum selva_io_flags act = io->flags & SELVA_IO_FLAGS_MODE_MASK;

    if (unlikely(!(act & (mode)))) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Incorrect IO mode: act: %d exp: %d", act, mode);
        exit(1);
    }
}

/**
 * Log a read error and exit.
 * @param whence should be one of WHENCE_ macros.
 */
#if __has_c_attribute(noreturn)
[[noreturn]]
#else
__attribute__((noreturn))
#endif
static void exit_read_error(const char *type, const char *whence)
{
    SELVA_LOG(SELVA_LOGL_CRIT, "Invalid read. whence: %s: type: %s", whence, type);
    exit(1);
}

int selva_io_new(const char *filename_str, size_t filename_len, enum selva_io_flags flags, struct selva_io **io_out)
{
    char *filename;
    struct selva_io *io;

    if (!(!(flags & SELVA_IO_FLAGS_READ) ^ !(flags & SELVA_IO_FLAGS_WRITE))) {
        return SELVA_EINVAL;
    }

    filename = alloca(filename_len + 1);
    memcpy(filename, filename_str, filename_len);
    filename[filename_len] = '\0';

    io = selva_malloc(sizeof(*io));
    io->flags = flags;
    io->err = 0;
    io->file = fopen(filename, "wb");
    if (!io->file) {
        /* TODO Better error handling */
        return SELVA_EGENERAL;
    }

    if (flags & SELVA_IO_FLAGS_WRITE) {
        fwrite_sdb_header(io);
    } else {
        fread_sdb_header(io);
    }

    *io_out = io;
    return 0;
}

void selva_io_end(struct selva_io *io)
{
    /* TODO hash */
    if (io->flags & SELVA_IO_FLAGS_WRITE) {
        fwrite_sdb_footer(io);
        fflush(io->file);
    } else {
        int err;

        err = fread_sdb_footer(io);
        if (err) {
            /* FIXME Handle error */
        }
    }

    fclose(io->file);
    selva_free(io);
}

void selva_io_save_unsigned(struct selva_io *io, uint64_t value)
{
    test_io_mode(io, SELVA_IO_FLAGS_WRITE);
    struct selva_proto_longlong buf = {
        .type = SELVA_PROTO_LONGLONG,
        .v = htole64(value),
    };

    fwrite(&buf, sizeof(buf), 1, io->file);
}

void selva_io_save_signed(struct selva_io *io, int64_t value)
{
    test_io_mode(io, SELVA_IO_FLAGS_WRITE);
    struct selva_proto_longlong buf = {
        .type = SELVA_PROTO_LONGLONG,
        .v = htole64(value),
    };

    fwrite(&buf, sizeof(buf), 1, io->file);
}

void selva_io_save_double(struct selva_io *io, double value)
{
    test_io_mode(io, SELVA_IO_FLAGS_WRITE);
    struct selva_proto_double buf = {
        .type = SELVA_PROTO_DOUBLE,
    };

    htoledouble((char *)&buf.v, value);

    fwrite(&buf, sizeof(buf), 1, io->file);
}

void selva_io_save_str(struct selva_io *io, const char *str, size_t len)
{
    test_io_mode(io, SELVA_IO_FLAGS_WRITE);
    struct selva_proto_string buf = {
        .type = SELVA_PROTO_STRING,
        .bsize = htole32(len),
    };

    fwrite(&buf, sizeof(buf), 1, io->file);
    fwrite(str, sizeof(char), len, io->file);
}

void selva_io_save_string(struct selva_io *io, const struct selva_string *s)
{
    test_io_mode(io, SELVA_IO_FLAGS_WRITE);
    size_t len;
    const char *str = selva_string_to_str(s, &len);
    struct selva_proto_string buf = {
        .type = SELVA_PROTO_STRING,
        .bsize = htole32(len),
    };

    fwrite(&buf, sizeof(buf), 1, io->file);
    fwrite(str, sizeof(char), len, io->file);
}

uint64_t selva_io_load_unsigned(struct selva_io *io)
{
    test_io_mode(io, SELVA_IO_FLAGS_READ);
    struct selva_proto_longlong buf;

    if (fread(&buf, sizeof(buf), 1, io->file) != sizeof(buf)) {
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_READ_HEADER);
    }

    if (buf.type != SELVA_PROTO_LONGLONG) {
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_HEADER_TYPE);
    }

    return letoh(buf.v);
}

int64_t selva_io_load_signed(struct selva_io *io)
{
    test_io_mode(io, SELVA_IO_FLAGS_READ);
    struct selva_proto_longlong buf;

    if (fread(&buf, sizeof(buf), 1, io->file) != sizeof(buf)) {
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_READ_HEADER);
    }

    if (buf.type != SELVA_PROTO_LONGLONG) {
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_HEADER_TYPE);
    }

    return (int64_t)letoh(buf.v);
}

double selva_io_load_double(struct selva_io *io)
{
    test_io_mode(io, SELVA_IO_FLAGS_READ);
    struct selva_proto_double buf;

    if (fread(&buf, sizeof(buf), 1, io->file) != sizeof(buf)) {
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_READ_HEADER);
    }

    if (buf.type != SELVA_PROTO_DOUBLE) {
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_HEADER_TYPE);
    }

    return buf.v; /* FIXME conversion */
}

const char *selva_io_load_str(struct selva_io *io, size_t *len)
{
    test_io_mode(io, SELVA_IO_FLAGS_READ);
    struct selva_proto_string buf;
    char *str;

    if (fread(&buf, sizeof(buf), 1, io->file) != sizeof(buf)) {
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_READ_HEADER);
    }

    if (buf.type != SELVA_PROTO_STRING) {
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_HEADER_TYPE);
    }

    buf.bsize = letoh(buf.bsize);
    str = selva_malloc(buf.bsize);
    if (fread(str, sizeof(char), buf.bsize, io->file) != buf.bsize * sizeof(char)) {
        selva_free(str);
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_VALUE);
    }

    if (len) {
        *len = buf.bsize;
    }

    return str;
}

struct selva_string *selva_io_load_string(struct selva_io *io)
{
    test_io_mode(io, SELVA_IO_FLAGS_READ);
    struct selva_proto_string buf;
    size_t len;
    struct selva_string *s;

    if (fread(&buf, sizeof(buf), 1, io->file) != sizeof(buf)) {
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_READ_HEADER);
    }

    if (buf.type != SELVA_PROTO_STRING) {
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_HEADER_TYPE);
    }

    len = letoh(buf.bsize);
    s = selva_string_create(NULL, len, SELVA_STRING_MUTABLE_FIXED);
    if (fread(selva_string_to_mstr(s, NULL), sizeof(char), len, io->file) != len * sizeof(char)) {
        selva_string_free(s);
        exit_read_error(selva_proto_typeof_str(buf), READ_WHENCE_VALUE);
    }

    return s;
}

IMPORT() {
    evl_import_main(selva_log);
}

__constructor static void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init io");
}
