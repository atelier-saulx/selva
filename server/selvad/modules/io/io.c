/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "endian.h"
#include "jemalloc.h"
#include "sha3iuf/sha3.h"
#include "util/selva_string.h"
#include "module.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_io.h"
#include "myreadlink.h"
#include "sdb.h"

#define READ_WHENCE_READ_HEADER "read header"
#define READ_WHENCE_HEADER_TYPE "type"
#define READ_WHENCE_VALUE       "read value"

static const char last_good_name[] = "dump.sdb";

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
static void exit_read_error(struct selva_io *io, const char *type, const char *whence)
{
    SELVA_LOG(SELVA_LOGL_CRIT, "Invalid read. offset: %ld whence: \"%s\" type: %s",
              ftell(io->file), whence, type);
    abort();
}

/**
 * Init an io structure for a file.
 * Note that flags must be validated before calling this function.
 */
static void init_io(struct selva_io *io, FILE *file, const char *filename, enum selva_io_flags flags)
{
    memset(io, 0, sizeof(*io));
    io->filename = selva_string_createf("%s", filename);
    io->file = file;
    io->flags = flags;
    sdb_init(io);

    if (flags & SELVA_IO_FLAGS_WRITE) {
        sdb_write_header(io);
    } else {
        sdb_read_header(io);
    }
}

int selva_io_open_last_good(struct selva_io *io)
{
    struct selva_string *filename;
    FILE *file;

    if (access(last_good_name, F_OK)) {
        return SELVA_ENOENT;
    }

    filename = myreadlink(last_good_name);
    if (!filename) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to resolve the symlink: \"%s\"", last_good_name);
        return SELVA_ENOENT;
    }

    file = fopen(selva_string_to_str(filename, NULL), "rb");
    if (!file) {
        /*
         * fopen() can fail for a dozen reasons, the best we can do is to tell
         * the caller that we failed to open the file.
         */
        return SELVA_EGENERAL;
    }

    init_io(io, file, selva_string_to_str(filename, NULL), SELVA_IO_FLAGS_READ);
    selva_string_free(filename);

    return 0;
}

int selva_io_last_good_info(uint8_t hash[SELVA_IO_HASH_SIZE], struct selva_string **filename_out)
{
    struct selva_io io;
    int err;

    selva_io_open_last_good(&io);
    fseek(io.file, -(SELVA_IO_HASH_SIZE + 8), SEEK_END);
    err = sdb_read_footer(&io);
    if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to read the last good file");
        selva_string_free(io.filename);
    } else {
        memcpy(hash, io.stored_hash, SELVA_IO_HASH_SIZE);
        *filename_out = io.filename;
    }

    fclose(io.file);

    return err;
}

int selva_io_init(struct selva_io *io, const char *filename, enum selva_io_flags flags)
{
    const char *mode = (flags & SELVA_IO_FLAGS_WRITE) ? "wb" : "rb";
    FILE *file;

    if (!(!(flags & SELVA_IO_FLAGS_READ) ^ !(flags & SELVA_IO_FLAGS_WRITE))) {
        return SELVA_EINVAL;
    }

    file = fopen(filename, mode);
    if (!file) {
        /*
         * fopen() can fail for a dozen reasons, the best we can do is to tell
         * the caller that we failed to open the file.
         */
        return SELVA_EGENERAL;
    }

    init_io(io, file, filename, flags);

    return 0;
}

static char *sha3_to_hex(char s[64], const uint8_t hash[SELVA_IO_HASH_SIZE])
{
    static const char map[] = "0123456789abcdef";
    char *p = s;

    for (size_t i = 0; i < SELVA_IO_HASH_SIZE; i++) {
        *p++ = map[(hash[i] >> 4) % sizeof(map)];
        *p++ = map[(hash[i] & 0x0f) % sizeof(map)];
    }

    return s;
}

void selva_io_end(struct selva_io *io)
{
    if (io->flags & SELVA_IO_FLAGS_WRITE) {
        sdb_write_footer(io);
        fflush(io->file);
    } else {
        int err;

        err = sdb_read_footer(io);
        if (!err && memcmp(io->computed_hash, io->stored_hash, SELVA_IO_HASH_SIZE)) {
            char act[64];
            char expected[64];

            SELVA_LOG(SELVA_LOGL_ERR, "Hash mismatch. act: %.*s. expected: %.*s",
                      64, sha3_to_hex(act, io->computed_hash),
                      64, sha3_to_hex(expected, io->stored_hash));
            err = SELVA_EINVAL;
        }
        if (err) {
            SELVA_LOG(SELVA_LOGL_CRIT, "SDB deserialization failed: %s", selva_strerror(err));
            /*
             * It wouldn't be necessary to crash here as hierarchy loading is
             * sort of safe to fail.
             */
            abort();
        }

    }

    fclose(io->file);

    /*
     * Mark as last good.
     */
    (void)unlink(last_good_name);
    if (symlink(selva_string_to_str(io->filename, NULL), last_good_name) == -1) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to symlink the last good dump \"%s\" as \"%s\"",
                  selva_string_to_str(io->filename, NULL), last_good_name);
    }

    selva_string_free(io->filename);
}

void selva_io_save_unsigned(struct selva_io *io, uint64_t value)
{
    test_io_mode(io, SELVA_IO_FLAGS_WRITE);
    struct selva_proto_longlong buf = {
        .type = SELVA_PROTO_LONGLONG,
        .v = htole64(value),
    };

    sdb_write(&buf, sizeof(buf), 1, io);
}

void selva_io_save_signed(struct selva_io *io, int64_t value)
{
    test_io_mode(io, SELVA_IO_FLAGS_WRITE);
    struct selva_proto_longlong buf = {
        .type = SELVA_PROTO_LONGLONG,
        .v = htole64(value),
    };

    sdb_write(&buf, sizeof(buf), 1, io);
}

void selva_io_save_double(struct selva_io *io, double value)
{
    test_io_mode(io, SELVA_IO_FLAGS_WRITE);
    struct selva_proto_double buf = {
        .type = SELVA_PROTO_DOUBLE,
    };

    htoledouble((char *)&buf.v, value);

    sdb_write(&buf, sizeof(buf), 1, io);
}

void selva_io_save_str(struct selva_io *io, const char *str, size_t len)
{
    test_io_mode(io, SELVA_IO_FLAGS_WRITE);
    struct selva_proto_string buf = {
        .type = SELVA_PROTO_STRING,
        .bsize = htole32(len),
    };

    sdb_write(&buf, sizeof(buf), 1, io);
    sdb_write(str, sizeof(char), len, io);
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

    sdb_write(&buf, sizeof(buf), 1, io);
    sdb_write(str, sizeof(char), len, io);
}

uint64_t selva_io_load_unsigned(struct selva_io *io)
{
    test_io_mode(io, SELVA_IO_FLAGS_READ);
    struct selva_proto_longlong buf;

    if (sdb_read(&buf, sizeof(buf), 1, io) != 1) {
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_READ_HEADER);
    }

    if (buf.type != SELVA_PROTO_LONGLONG) {
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_HEADER_TYPE);
    }

    return letoh(buf.v);
}

int64_t selva_io_load_signed(struct selva_io *io)
{
    test_io_mode(io, SELVA_IO_FLAGS_READ);
    struct selva_proto_longlong buf;

    if (sdb_read(&buf, sizeof(buf), 1, io) != 1) {
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_READ_HEADER);
    }

    if (buf.type != SELVA_PROTO_LONGLONG) {
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_HEADER_TYPE);
    }

    return (int64_t)letoh(buf.v);
}

double selva_io_load_double(struct selva_io *io)
{
    test_io_mode(io, SELVA_IO_FLAGS_READ);
    struct selva_proto_double buf;

    if (sdb_read(&buf, sizeof(buf), 1, io) != 1) {
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_READ_HEADER);
    }

    if (buf.type != SELVA_PROTO_DOUBLE) {
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_HEADER_TYPE);
    }

    return ledoubletoh((void *)&buf.v);
}

const char *selva_io_load_str(struct selva_io *io, size_t *len)
{
    test_io_mode(io, SELVA_IO_FLAGS_READ);
    struct selva_proto_string buf;
    char *str;

    if (sdb_read(&buf, sizeof(buf), 1, io) != 1) {
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_READ_HEADER);
    }

    if (buf.type != SELVA_PROTO_STRING) {
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_HEADER_TYPE);
    }

    buf.bsize = letoh(buf.bsize);
    str = selva_malloc(buf.bsize);
    if (sdb_read(str, sizeof(char), buf.bsize, io) != buf.bsize * sizeof(char)) {
        selva_free(str);
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_VALUE);
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

    if (sdb_read(&buf, sizeof(buf), 1, io) != 1) {
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_READ_HEADER);
    }

    if (buf.type != SELVA_PROTO_STRING) {
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_HEADER_TYPE);
    }

    len = letoh(buf.bsize);
    s = selva_string_create(NULL, len, SELVA_STRING_MUTABLE_FIXED);
    if (sdb_read(selva_string_to_mstr(s, NULL), sizeof(char), len, io) != len * sizeof(char)) {
        selva_string_free(s);
        exit_read_error(io, selva_proto_typeof_str(buf), READ_WHENCE_VALUE);
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
