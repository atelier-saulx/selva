/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sha3iuf/sha3.h"
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

void sdb_init(struct selva_io *io)
{
    sha3_Init256(&io->hash_c);
}

size_t sdb_write(const void * ptr, size_t size, size_t count, struct selva_io *io)
{
    sha3_Update(&io->hash_c, ptr, count * size);
    return fwrite(ptr, size, count, io->file);
}

size_t sdb_read(void * ptr, size_t size, size_t count, struct selva_io *io)
{
    size_t r;

    r = fread(ptr, size, count, io->file);
    sha3_Update(&io->hash_c, ptr, count * size);

    return r;
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

    sdb_write(magic_start, sizeof(char), sizeof(magic_start), io);
    sdb_write(created_with, sizeof(char), SELVA_DB_VERSION_SIZE, io);
    sdb_write(selva_db_version_info.running, sizeof(char), SELVA_DB_VERSION_SIZE, io); /* updated_with */
    sdb_write(pad, sizeof(char), sizeof(pad), io);
    if (ferror(io->file)) {
        return SELVA_EIO;
    }

    return 0;
}

int sdb_read_header(struct selva_io *io)
{
    char magic[sizeof(magic_start)];
    char pad[8];
    size_t res;

    res = sdb_read(magic, sizeof(char), sizeof(magic), io);
    if (res != sizeof(magic) || memcmp(magic, magic_start, sizeof(magic))) {
        return SELVA_EINVAL;
    }

    res = sdb_read(selva_db_version_info.created_with, SELVA_DB_VERSION_SIZE, 1, io);
    res += sdb_read(selva_db_version_info.updated_with, SELVA_DB_VERSION_SIZE, 1, io);
    res += sdb_read(pad, sizeof(char), sizeof(pad), io);
    if (res != 2 + sizeof(pad)) {
        return SELVA_EINVAL;
    }

    SELVA_LOG(SELVA_LOGL_INFO,
              "sdb loading. created_with: %.*s updated_with: %.*s",
              SELVA_DB_VERSION_SIZE, selva_db_version_info.created_with,
              SELVA_DB_VERSION_SIZE, selva_db_version_info.updated_with);

    return 0;
}

int sdb_write_footer(struct selva_io *io)
{
    sdb_write(magic_end, sizeof(char), sizeof(magic_end), io);

    io->computed_hash = sha3_Finalize(&io->hash_c);
    fwrite(io->computed_hash, sizeof(uint8_t), SELVA_IO_HASH_SIZE, io->file);
    if (ferror(io->file)) {
        return SELVA_EIO;
    }

    return 0;
}

static char *sha3_to_hex(char s[64], const uint8_t hash[SELVA_IO_HASH_SIZE])
{
    static const char map[] = "0123456789abcdef";
    char *p = s;

    for (size_t i = 0; i < SELVA_IO_HASH_SIZE; i++) {
        *p++ = map[(hash[i] >> 4) % 16];
        *p++ = map[(hash[i] & 0x0f) % 16];
    }

    return s;
}

int sdb_read_footer(struct selva_io *io)
{
    char magic[sizeof(magic_start)];
    uint8_t stored_hash[SELVA_IO_HASH_SIZE];
    size_t res;

    res = sdb_read(magic, sizeof(char), sizeof(magic), io);
    if (res != sizeof(magic) || memcmp(magic, magic_end, sizeof(magic))) {
        SELVA_LOG(SELVA_LOGL_ERR, "Bad magic");
        return SELVA_EINVAL;
    }

    io->computed_hash = sha3_Finalize(&io->hash_c);
    res = fread(stored_hash, sizeof(uint8_t), sizeof(stored_hash), io->file);
    if (res != SELVA_IO_HASH_SIZE) {
        SELVA_LOG(SELVA_LOGL_ERR, "Hash size invalid. act: %zu expected: %zu", res, (size_t)SELVA_IO_HASH_SIZE);
    }
    if (memcmp(io->computed_hash, stored_hash, SELVA_IO_HASH_SIZE)) {
        char act[64];
        char expected[64];

        SELVA_LOG(SELVA_LOGL_ERR, "Hash mismatch. act: %.*s. expected: %.*s",
                  64, sha3_to_hex(act, io->computed_hash),
                  64, sha3_to_hex(expected, stored_hash));
        return SELVA_EINVAL;
    }

    return 0;
}

__constructor static void init(void)
{
    strncpy(selva_db_version_info.running, selva_db_version, min((int)strlen(selva_db_version), SELVA_DB_VERSION_SIZE));

    SELVA_LOG(SELVA_LOGL_INFO, "Selva db version running: %.*s", SELVA_DB_VERSION_SIZE, selva_db_version_info.running);
}
