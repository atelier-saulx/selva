/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

int fwrite_sdb_header(struct selva_io *io)
{
    const char *created_with;

    if (selva_db_version_info.created_with[0] != '\0') {
        created_with = selva_db_version_info.created_with;
    } else {
        created_with = selva_db_version_info.running;
    }

    fwrite(magic_start, sizeof(char), sizeof(magic_start), io->file);
    fwrite(created_with, sizeof(char), SELVA_DB_VERSION_SIZE, io->file);
    fwrite(selva_db_version_info.running, sizeof(char), SELVA_DB_VERSION_SIZE, io->file); /* updated_with */

    /* TODO Error handling */

    return 0;
}

int fread_sdb_header(struct selva_io *io)
{
    char magic[sizeof(magic_start)];
    size_t res;

    res = fread(magic, sizeof(char), sizeof(magic), io->file);
    if (res != sizeof(magic) || memcmp(magic, magic_start, sizeof(magic))) {
        return SELVA_EINVAL; /* TODO Better error code */
    }

    res = fread(selva_db_version_info.created_with, SELVA_DB_VERSION_SIZE, 1, io->file);
    res += fread(selva_db_version_info.updated_with, SELVA_DB_VERSION_SIZE, 1, io->file);
    if (res != 2) {
        return SELVA_EINVAL; /* TODO Better error code */
    }

    SELVA_LOG(SELVA_LOGL_INFO,
              "sdb loading. created_with: %.*s updated_with: %.*s",
              SELVA_DB_VERSION_SIZE, selva_db_version_info.created_with,
              SELVA_DB_VERSION_SIZE, selva_db_version_info.updated_with);

    return 0;
}

int fwrite_sdb_footer(struct selva_io *io)
{
    fwrite(magic_end, sizeof(char), sizeof(magic_end), io->file);
    /* TODO Update hash */
    fwrite(io->hash, sizeof(char), sizeof(io->hash), io->file);

    return 0;
}

int fread_sdb_footer(struct selva_io *io)
{
    char magic[sizeof(magic_start)];
    char hash[sizeof(io->hash)];
    size_t res;

    res = fread(magic, sizeof(char), sizeof(magic), io->file);
    if (res != sizeof(magic) || memcmp(magic, magic_end, sizeof(magic))) {
        return SELVA_EINVAL; /* TODO Better error code */
    }

    res = fread(hash, sizeof(char), sizeof(hash), io->file);
    if (res != sizeof(hash) || memcmp(hash, io->hash, sizeof(hash))) {
        return SELVA_EINVAL; /* TODO Better error code */
    }

    return 0;
}

__constructor static void init(void)
{
    strncpy(selva_db_version_info.running, selva_db_version, min((int)strlen(selva_db_version), SELVA_DB_VERSION_SIZE));

    SELVA_LOG(SELVA_LOGL_INFO, "Selva db version running: %.*s", SELVA_DB_VERSION_SIZE, selva_db_version_info.running);
}
