/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "jemalloc.h"
#include "selva_error.h"
#include "selva_log.h"
#include "module.h"
#include "selva_io.h"
#include "sdb.h"

/* TODO Better error code? */
#define TEST_IO_MODE(io, mode) \
    do { \
        enum selva_io_flags act = (io)->flags & SELVA_IO_FLAGS_MODE_MASK; \
        if (unlikely(!(act & (mode)))) { \
            SELVA_LOG(SELVA_LOGL_CRIT, "Incorrect IO mode: act: %d exp: %d", act, mode); \
            exit(1); \
        } \
    } while (0)

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
    fwrite_sdb_footer(io);
    fflush(io->file);
    fclose(io->file);
    selva_free(io);
}

void selva_io_save_unsigned(struct selva_io *io, uint64_t value)
{
    TEST_IO_MODE(io, SELVA_IO_FLAGS_WRITE);
}

void selva_io_save_signed(struct selva_io *io, int64_t value)
{
    TEST_IO_MODE(io, SELVA_IO_FLAGS_WRITE);
}

void selva_io_save_double(struct selva_io *io, double d)
{
    TEST_IO_MODE(io, SELVA_IO_FLAGS_WRITE);
}

void selva_io_save_str(struct selva_io *io, const char *str, size_t len)
{
    TEST_IO_MODE(io, SELVA_IO_FLAGS_WRITE);
}

void selva_io_save_string(struct selva_io *io, const struct selva_string *s)
{
    TEST_IO_MODE(io, SELVA_IO_FLAGS_WRITE);
}

uint64_t selva_io_load_unsigned(struct selva_io *io)
{
    TEST_IO_MODE(io, SELVA_IO_FLAGS_READ);

    return 0;
}

int64_t selva_io_load_signed(struct selva_io *io)
{
    TEST_IO_MODE(io, SELVA_IO_FLAGS_READ);

    return 0;
}

double selva_io_load_double(struct selva_io *io)
{
    TEST_IO_MODE(io, SELVA_IO_FLAGS_READ);

    return 0.0;
}

const char *selva_io_load_str(struct selva_io *io, size_t *len)
{
    TEST_IO_MODE(io, SELVA_IO_FLAGS_READ);

    return NULL;
}

struct selva_string *selva_io_load_string(struct selva_io *io)
{
    TEST_IO_MODE(io, SELVA_IO_FLAGS_READ);

    return NULL;
}

IMPORT() {
    evl_import_main(selva_log);
}

__constructor static void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init io");
}
