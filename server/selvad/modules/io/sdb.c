/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sha3iuf/sha3.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_io.h"
#include "sdb.h"

extern const char * const selva_db_version;
static const char magic_start[] = { 'S', 'E', 'L', 'V', 'A', '\0', '\0', '\0' };
static const char magic_end[]   = { '\0', '\0', '\0', 'A', 'V', 'L', 'E', 'S' };

/**
 * Selva module version tracking.
 * This is used to track the Selva module version used to create and modify the
 * hierarchy that was serialized and later deserialized.
 */
static struct SelvaDbVersionInfo selva_db_version_info;

void selva_io_get_ver(struct SelvaDbVersionInfo *nfo)
{
    memcpy(nfo, &selva_db_version_info, sizeof(*nfo));
}

static size_t sdb_write_file(const void * restrict ptr, size_t size, size_t count, struct selva_io *restrict io)
{
    sha3_Update(&io->hash_c, ptr, count * size);
    return fwrite(ptr, size, count, io->file_io.file);
}

static size_t sdb_read_file(void * restrict ptr, size_t size, size_t count, struct selva_io *restrict io)
{
    size_t r;

    r = fread(ptr, size, count, io->file_io.file);
    sha3_Update(&io->hash_c, ptr, count * size);

    return r;
}

static size_t sdb_write_string(const void * restrict ptr, size_t size, size_t count, struct selva_io * restrict io)
{
    int err;

    sha3_Update(&io->hash_c, ptr, count * size);
    err = selva_string_append(io->string_io.data, ptr, size * count);

    if (err) {
        io->string_io.err = err;
        return 0;
    } else {
        return count;
    }
}

static size_t sdb_read_string(void * restrict ptr, size_t size, size_t count, struct selva_io * restrict io)
{
    const char *data;
    size_t data_len;
    const size_t rd = size * count;

    data = selva_string_to_str(io->string_io.data, &data_len);

    if (io->string_io.offset + rd > data_len) {
        return 0;
    }

    memcpy(ptr, data + io->string_io.offset, rd);
    io->string_io.offset += rd;

    sha3_Update(&io->hash_c, ptr, rd);

    return rd;
}

static off_t sdb_tell_file(struct selva_io *io)
{
    return ftello(io->file_io.file);
}

static off_t sdb_tell_string(struct selva_io *io)
{
    return (off_t)io->string_io.offset;
}

static int sdb_seek_file(struct selva_io *io, off_t offset, int whence)
{
    return fseeko(io->file_io.file, offset, whence);
}

static int sdb_seek_string(struct selva_io *io, off_t offset, int whence)
{
    size_t data_len;

    (void)selva_string_to_str(io->string_io.data, &data_len);
    if (whence == SEEK_SET) {
        /* NOP */
    } else if (whence == SEEK_CUR) {
        offset += io->string_io.offset;
    } else if (whence == SEEK_END) {
        offset = data_len + io->string_io.offset;
    } else {
        return SELVA_EINVAL;
    }

    if ((size_t)offset > data_len) {
        return SELVA_EIO;
    }

    io->string_io.offset = (size_t)offset;
    return 0;
}

static int sdb_error_file(struct selva_io *restrict io)
{
    if (ferror(io->file_io.file)) {
        return SELVA_EIO;
    }

    return 0;
}

static int sdb_error_string(struct selva_io *restrict io)
{
    return io->string_io.err;
}

static void sdb_clearerr_file(struct selva_io *restrict io)
{
    clearerr(io->file_io.file);
}

static void sdb_clearerr_string(struct selva_io *restrict io)
{
    io->string_io.err = 0;
}

void sdb_init(struct selva_io *io)
{
    sha3_Init256(&io->hash_c);
    if (io->flags & SELVA_IO_FLAGS_FILE_IO) {
        io->sdb_write = sdb_write_file;
        io->sdb_read = sdb_read_file;
        io->sdb_tell = sdb_tell_file;
        io->sdb_seek = sdb_seek_file;
        io->sdb_error = sdb_error_file;
        io->sdb_clearerr = sdb_clearerr_file;
    } else if (io->flags & SELVA_IO_FLAGS_STRING_IO) {
        io->sdb_write = sdb_write_string;
        io->sdb_read = sdb_read_string;
        io->sdb_tell = sdb_tell_string;
        io->sdb_seek = sdb_seek_string;
        io->sdb_error = sdb_error_string;
        io->sdb_clearerr = sdb_clearerr_string;
    }
}

int sdb_write_header(struct selva_io *io)
{
    const char *created_with;
    const char pad[8] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };

    if (selva_db_version_info.created_with[0] != '\0') {
        created_with = selva_db_version_info.created_with;
    } else {
        created_with = selva_db_version_info.running;
    }

    io->sdb_write(magic_start, sizeof(char), sizeof(magic_start), io);
    io->sdb_write(created_with, sizeof(char), SELVA_DB_VERSION_SIZE, io);
    io->sdb_write(selva_db_version_info.running, sizeof(char), SELVA_DB_VERSION_SIZE, io); /* updated_with */
    io->sdb_write(pad, sizeof(char), sizeof(pad), io);
    return io->sdb_error(io);
}

int sdb_read_header(struct selva_io *io)
{
    char magic[sizeof(magic_start)];
    char pad[8];
    size_t res;

    res = io->sdb_read(magic, sizeof(char), sizeof(magic), io);
    if (res != sizeof(magic) || memcmp(magic, magic_start, sizeof(magic))) {
        return SELVA_EINVAL;
    }

    res = io->sdb_read(selva_db_version_info.created_with, SELVA_DB_VERSION_SIZE, 1, io);
    res += io->sdb_read(selva_db_version_info.updated_with, SELVA_DB_VERSION_SIZE, 1, io);
    res += io->sdb_read(pad, sizeof(char), sizeof(pad), io);
    if (res != 2 + sizeof(pad)) {
        return SELVA_EINVAL;
    }

    SELVA_LOG(SELVA_LOGL_INFO,
              "created_with: %.*s updated_with: %.*s",
              SELVA_DB_VERSION_SIZE, selva_db_version_info.created_with,
              SELVA_DB_VERSION_SIZE, selva_db_version_info.updated_with);

    return 0;
}

int sdb_write_footer(struct selva_io *io)
{
    int err;

    io->sdb_write(magic_end, sizeof(char), sizeof(magic_end), io);

    io->computed_hash = sha3_Finalize(&io->hash_c);
    if (io->flags & SELVA_IO_FLAGS_FILE_IO) {
        fwrite(io->computed_hash, sizeof(uint8_t), SELVA_IO_HASH_SIZE, io->file_io.file);
        err = io->sdb_error(io);
    } else if (io->flags & SELVA_IO_FLAGS_STRING_IO) {
        err = selva_string_append(io->string_io.data, (void *)io->computed_hash, SELVA_IO_HASH_SIZE);
    }

    return err;
}

int sdb_read_footer(struct selva_io *io)
{
    char magic[sizeof(magic_end)];
    size_t res;

    res = io->sdb_read(magic, sizeof(char), sizeof(magic), io);
    if (res != sizeof(magic) || memcmp(magic, magic_end, sizeof(magic))) {
        SELVA_LOG(SELVA_LOGL_ERR, "Bad magic: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x",
                  (uint8_t)magic[0], (uint8_t)magic[1],
                  (uint8_t)magic[2], (uint8_t)magic[3],
                  (uint8_t)magic[4], (uint8_t)magic[5],
                  (uint8_t)magic[6], (uint8_t)magic[7]);
        return SELVA_EINVAL;
    }

    io->computed_hash = sha3_Finalize(&io->hash_c);
    if (io->flags & SELVA_IO_FLAGS_FILE_IO) {
        res = fread(io->stored_hash, sizeof(uint8_t), sizeof(io->stored_hash), io->file_io.file);
        if (res != SELVA_IO_HASH_SIZE) {
            SELVA_LOG(SELVA_LOGL_ERR, "Hash size invalid. act: %zu expected: %zu", res, (size_t)SELVA_IO_HASH_SIZE);
            return SELVA_EINVAL;
        }
    } else if (io->flags & SELVA_IO_FLAGS_STRING_IO) {
        const char *data;
        size_t data_len;
        const size_t rd = (size_t)SELVA_IO_HASH_SIZE;

        data = selva_string_to_str(io->string_io.data, &data_len);

        if (io->string_io.offset + rd > data_len) {
            return SELVA_EINVAL;
        }

        memcpy(io->stored_hash, data + io->string_io.offset, rd);
    }

    return 0;
}

__constructor static void init(void)
{
    strncpy(selva_db_version_info.running, selva_db_version, min((int)strlen(selva_db_version), SELVA_DB_VERSION_SIZE));

    SELVA_LOG(SELVA_LOGL_INFO, "Selva db version running: %.*s", SELVA_DB_VERSION_SIZE, selva_db_version_info.running);
}
