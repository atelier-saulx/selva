/*
 * Selva IO Module.
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "_evl_export.h"
#include "_selva_io.h"

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
    SELVA_IO_FLAGS_FILE_IO = 0x10, /*! Save to/Load from a file. Not set by caller. */
    SELVA_IO_FLAGS_STRING_IO = 0x20, /*!< Save to/Load from a file. Not set by caller. */
};

#ifdef SELVA_IO_TYPE
struct selva_io {
    enum selva_io_flags flags;
    union {
        struct {
            struct selva_string *filename;
            FILE *file;
        } file_io;
        struct {
            int err;
            size_t offset;
            struct selva_string *data;
        } string_io;
    };

    sha3_context hash_c; /*!< Currently computed hash of the data. */
    const uint8_t *computed_hash; /*!< Updated at the end of load/save. */
    uint8_t stored_hash[SELVA_IO_HASH_SIZE]; /*!< The hash found in the footer. */

    size_t (*sdb_write)(const void * restrict ptr, size_t size, size_t count, struct selva_io * restrict io);
    size_t (*sdb_read)(void * restrict ptr, size_t size, size_t count, struct selva_io *restrict io);
    off_t (*sdb_tell)(struct selva_io *io);
    int (*sdb_seek)(struct selva_io *io, off_t offset, int whence);
    int (*sdb_flush)(struct selva_io *io);

    /**
     * Return the last IO error.
     */
    int (*sdb_error)(struct selva_io *restrict io);
    void (*sdb_clearerr)(struct selva_io *restrict io);
};
#endif

#define SELVA_IO_FLAGS_MODE_MASK (SELVA_IO_FLAGS_READ | SELVA_IO_FLAGS_WRITE)

/*
 * TODO Move string compression to util
 * TODO Move hierarchy compression stuff to selva_io
 */

SELVA_IO_EXPORT(void, selva_io_get_ver, struct SelvaDbVersionInfo *nfo);

/**
 * Open the last good SDB for reading.
 */
SELVA_IO_EXPORT(int, selva_io_open_last_good, struct selva_io *io);

SELVA_IO_EXPORT(int, selva_io_last_good_info, uint8_t hash[SELVA_IO_HASH_SIZE], struct selva_string **filename_out);
SELVA_IO_EXPORT(int, selva_io_read_hash, const char *filename, uint8_t hash[SELVA_IO_HASH_SIZE]);

/**
 * Start a new IO operation.
 * @param io is a pointer to the io state. Can be allocated from the stack.
 */
SELVA_IO_EXPORT(int, selva_io_init, struct selva_io *io, const char *filename, enum selva_io_flags flags);

/**
 * Start a new IO operation writing to a selva_string.
 * @param io is a pointer to the io state. Can be allocated from the stack.
 * @returns the selva string that will be appended.
 */
SELVA_IO_EXPORT(struct selva_string *, selva_io_init_string_write, struct selva_io *io, enum selva_io_flags flags);


/**
 * Start a new IO operation reading from a selva_string.
 * @param io is a pointer to the io state. Can be allocated from the stack.
 */
SELVA_IO_EXPORT(int, selva_io_init_string_read, struct selva_io * restrict io, struct selva_string * restrict s, enum selva_io_flags flags);

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

#define _import_selva_io(apply) \
    apply(selva_io_get_ver) \
    apply(selva_io_open_last_good) \
    apply(selva_io_last_good_info) \
    apply(selva_io_read_hash) \
    apply(selva_io_init) \
    apply(selva_io_init_string_write) \
    apply(selva_io_init_string_read) \
    apply(selva_io_end) \
    apply(selva_io_save_unsigned) \
    apply(selva_io_save_signed) \
    apply(selva_io_save_double) \
    apply(selva_io_save_str) \
    apply(selva_io_save_string) \
    apply(selva_io_load_unsigned) \
    apply(selva_io_load_signed) \
    apply(selva_io_load_double) \
    apply(selva_io_load_str) \
    apply(selva_io_load_string)

#define _import_selva_io1(f) \
    evl_import(f, "mod_io.so");

/**
 * Import all symbols from selva_io.h.
 */
#define import_selva_io() \
    _import_selva_io(_import_selva_io1)
