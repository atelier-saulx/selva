/*
 * Selva IO Module.
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "_evl_export.h"

#if SELVA_IO_MAIN
#define SELVA_IO_EXPORT(_ret_, _fun_name_, ...) _ret_ _fun_name_(__VA_ARGS__) EVL_EXTERN
#else
#define SELVA_IO_EXPORT(_ret_, _fun_name_, ...) _ret_ (*_fun_name_)(__VA_ARGS__) EVL_COMMON
#endif

struct selva_io;
struct selva_string;

#define SELVA_DB_VERSION_SIZE   40
struct SelvaDbVersionInfo {
    char running[SELVA_DB_VERSION_SIZE] __attribute__((nonstring));
    char created_with[SELVA_DB_VERSION_SIZE] __attribute__((nonstring));
    char updated_with[SELVA_DB_VERSION_SIZE] __attribute__((nonstring));
};

enum selva_io_flags {
    SELVA_IO_FLAGS_READ = 0x01,
    SELVA_IO_FLAGS_WRITE = 0x02,
    SELVA_IO_FLAGS_COMPRESSED = 0x04, /* TODO */
};

#define SELVA_IO_FLAGS_MODE_MASK (SELVA_IO_FLAGS_READ | SELVA_IO_FLAGS_WRITE)

/*
 * TODO Move string compression to util
 * TODO Move hierarchy compression stuff to selva_io
 */

SELVA_IO_EXPORT(void, selva_io_get_ver, struct SelvaDbVersionInfo *nfo);

/**
 * Start a new IO operation.
 */
SELVA_IO_EXPORT(int, selva_io_new, const char *filename_str, size_t filename_len, enum selva_io_flags flags, struct selva_io **io_out);

/**
 * End the IO operation.
 * 1. Verifies the hash on read mode; Writes the hash in write mode.
 * 2. Closes the file.
 */
SELVA_IO_EXPORT(void, selva_io_end, struct selva_io *io);

SELVA_IO_EXPORT(void, selva_io_save_unsigned, struct selva_io *io, uint64_t value);
SELVA_IO_EXPORT(void, selva_io_save_signed, struct selva_io *io, int64_t value);
SELVA_IO_EXPORT(void, selva_io_save_double, struct selva_io *io, double value);
SELVA_IO_EXPORT(void, selva_io_save_str, struct selva_io *io, const char *str, size_t len);
SELVA_IO_EXPORT(void, selva_io_save_string, struct selva_io *io, const struct selva_string *s);

SELVA_IO_EXPORT(uint64_t, selva_io_load_unsigned, struct selva_io *io);
SELVA_IO_EXPORT(int64_t, selva_io_load_signed, struct selva_io *io);
SELVA_IO_EXPORT(double, selva_io_load_double, struct selva_io *io);
SELVA_IO_EXPORT(const char*, selva_io_load_str, struct selva_io *io, size_t *len);
SELVA_IO_EXPORT(struct selva_string *, selva_io_load_string, struct selva_io *io);
