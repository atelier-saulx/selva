/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _SELVA_COMPRESSOR_H_
#define _SELVA_COMPRESSOR_H_

struct selva_io;
struct selva_string;

/**
 * @addtogroup compressor
 * @{
 */

/**
 * Compressed selva_string.
 */
struct compressed_string {
    ssize_t uncompressed_size; /*!< if equal or greater than zero then the string is compressed; Otherwise the string is uncompressed. */
    struct selva_string *s;
};

/**
 * Allocate a compressed_string structure.
 * @returns a new compressed_string structure.
 */
__attribute__ ((malloc)) static inline struct compressed_string *alloc_compressed_string(void) {
    return selva_calloc(1, sizeof(struct compressed_string));
}

/**
 * Free a compressed selva_string.
 */
static inline void free_compressed_string(struct compressed_string *compressed_string) {
    if (compressed_string->s) {
        selva_string_free(compressed_string->s);
    }
    selva_free(compressed_string);
}

/**
 * Compress selva_string `in` to `out`.
 * @returns a Selva error code is returned.
 */
int compress_string(struct compressed_string *out, struct selva_string *in, double *cratio);

/**
 * Decompress `in` and return it as `out`.
 * @returns a Selva error code is returned.
 */
int decompress_string(struct selva_string **out, struct compressed_string *in);

/**
 * Write a compressed_string to fp.
 * Note that the binary format is not portable and it's only assumed to be read
 * by the same process (on the same machine) that wrote it.
 */
int fwrite_compressed_string(const struct compressed_string *compressed, FILE *fp);

/**
 * Read a compressed_string from fp.
 * Note that the binary format is not portable and it's only assumed to be read
 * by the same process (on the same machine) that wrote it.
 */
int fread_compressed_string(struct compressed_string *compressed, FILE *fp);

/**
 * RDB save a compressed string.
 */
void RDBSaveCompressedString(struct selva_io *io, struct compressed_string *compressed);

/**
 * RDB load a compressed string.
 */
void RDBLoadCompressedString(struct selva_io *io, struct compressed_string *compressed);

/**
 * @}
 */

#endif /* _SELVA_COMPRESSOR_H_ */
