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

#define HASH_SIZE 32

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

size_t sdb_write(const void * ptr, size_t size, size_t count, struct selva_io *io)
{
    sha3_Update(&io->hash_c, ptr, count * size);
    return fwrite(ptr, size, count, io->file);
}

size_t sdb_read(void * ptr, size_t size, size_t count, struct selva_io *io)
{
    sha3_Update(&io->hash_c, ptr, count * size);
    return fread(ptr, size, count, io->file);
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
    /* TODO Error handling */

    return 0;
}

int sdb_read_header(struct selva_io *io)
{
    char magic[sizeof(magic_start)];
    size_t res;

    res = sdb_read(magic, sizeof(char), sizeof(magic), io);
    if (res != sizeof(magic) || memcmp(magic, magic_start, sizeof(magic))) {
        return SELVA_EINVAL; /* TODO Better error code */
    }

    res = sdb_read(selva_db_version_info.created_with, SELVA_DB_VERSION_SIZE, 1, io);
    res += sdb_read(selva_db_version_info.updated_with, SELVA_DB_VERSION_SIZE, 1, io);
    if (res != 2) {
        return SELVA_EINVAL; /* TODO Better error code */
    }
    fseek(io->file, 8, SEEK_CUR); /* Skip pad */

    SELVA_LOG(SELVA_LOGL_INFO,
              "sdb loading. created_with: %.*s updated_with: %.*s",
              SELVA_DB_VERSION_SIZE, selva_db_version_info.created_with,
              SELVA_DB_VERSION_SIZE, selva_db_version_info.updated_with);

    return 0;
}

int sdb_write_footer(struct selva_io *io)
{
    const uint8_t *computed_hash;

    computed_hash = sha3_Finalize(&io->hash_c);
    sdb_write(magic_end, sizeof(char), sizeof(magic_end), io);
    fwrite(computed_hash, sizeof(uint8_t), HASH_SIZE, io->file);

    return 0;
}

int sdb_read_footer(struct selva_io *io)
{
    char magic[sizeof(magic_start)];
    uint8_t stored_hash[HASH_SIZE];
    const uint8_t *computed_hash;
    size_t res;

    res = sdb_read(magic, sizeof(char), sizeof(magic), io);
    if (res != sizeof(magic) || memcmp(magic, magic_end, sizeof(magic))) {
        return SELVA_EINVAL; /* TODO Better error code */
    }

    computed_hash = sha3_Finalize(&io->hash_c);
    res = fread(stored_hash, sizeof(uint8_t), sizeof(stored_hash), io->file);
    if (res != HASH_SIZE || memcmp(computed_hash, stored_hash, HASH_SIZE)) {
        return SELVA_EINVAL; /* TODO Better error code */
    }

    return 0;
}

__constructor static void init(void)
{
    strncpy(selva_db_version_info.running, selva_db_version, min((int)strlen(selva_db_version), SELVA_DB_VERSION_SIZE));

    SELVA_LOG(SELVA_LOGL_INFO, "Selva db version running: %.*s", SELVA_DB_VERSION_SIZE, selva_db_version_info.running);
}
