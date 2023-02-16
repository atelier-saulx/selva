/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libdeflate.h"
#include "jemalloc.h"
#include "util/auto_free.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_io.h"
#include "db_config.h"
#include "selva_onload.h"
#include "compressor.h"

static struct libdeflate_compressor *compressor;
static struct libdeflate_decompressor *decompressor;

int compress_string(struct compressed_string *out, struct selva_string *in, double *cratio) {
    TO_STR(in);
    struct selva_string *compressed = selva_string_create(NULL, in_len, SELVA_STRING_MUTABLE);
    size_t compressed_size;

    compressed_size = libdeflate_deflate_compress(compressor, in_str, in_len, selva_string_to_mstr(compressed, NULL), in_len);
    if (compressed_size == 0) {
        /*
         * No compression was achieved.
         * Therefore we use the original uncompressed string.
         */
        selva_string_free(compressed);
        compressed_size = in_len;
        out->uncompressed_size = -1;
        out->s = selva_string_dup(in, 0);
    } else {
        /*
         * The string was compressed.
         */
        selva_string_truncate(compressed, compressed_size);
        out->uncompressed_size = in_len;
        out->s = compressed;
    }

    if (cratio) {
        *cratio = (double)in_len / (double)compressed_size;
    }

    return 0;
}

int decompress_string(struct selva_string **out, struct compressed_string *in) {
    size_t compressed_len;
    const char *compressed_str;
    struct selva_string *uncompressed;
    size_t nbytes_out = 0;
    enum libdeflate_result res;

    if (in->uncompressed_size < 0) {
        /* RFE dup is a bit dumb in this case? */
        *out = selva_string_dup(in->s, 0);
        return 0;
    }

    compressed_str = selva_string_to_str(in->s, &compressed_len);
    uncompressed = selva_string_create(NULL, in->uncompressed_size, SELVA_STRING_MUTABLE);
    res = libdeflate_deflate_decompress(decompressor, compressed_str, compressed_len, selva_string_to_mstr(uncompressed, NULL), in->uncompressed_size, &nbytes_out);
    if (res != 0 || nbytes_out != (size_t)in->uncompressed_size) {
        selva_string_free(uncompressed);
        return SELVA_EINVAL;
    }

    *out = uncompressed;
    return 0;
}

int fwrite_compressed_string(const struct compressed_string *compressed, FILE *fp) {
    size_t buf_size;
    const char *buf = selva_string_to_str(compressed->s, &buf_size);
    size_t res;

    res = fwrite(&compressed->uncompressed_size, sizeof(compressed->uncompressed_size), 1, fp);
    if (res != 1 && ferror(fp)) {
        return SELVA_EGENERAL;
    }

    res = fwrite(buf, sizeof(char), buf_size, fp);
    if (res != buf_size && ferror(fp)) {
        return SELVA_ENOBUFS;
    }

    return 0;
}

static ssize_t get_file_size(FILE *fp) {
    long int size;

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    return size == -1L ? SELVA_EGENERAL : (ssize_t)size;
}

const char *get_filename(char filename[255], FILE *fp) {
    static const char unknown_filename[] = "<unknown file>";
#if __linux__
    int fd;
    char fd_path[255];
    ssize_t n;

    fd = fileno(fp);
    if (fd == -1) {
        return unknown_filename;
    }

    sprintf(fd_path, "/proc/self/fd/%d", fd);
    n = readlink(fd_path, filename, 255);
    if (n < 0) {
        return unknown_filename;
    }

    filename[n] = '\0';
    return filename;
#else
    return unknown_filename;
#endif
}

static void print_read_error(FILE *fp) {
    char filename[255];
    const int ferr = ferror(fp);
    const char *str_err = ferr ? strerror(errno) : "No error";
    const int eof = feof(fp);

    SELVA_LOG(SELVA_LOGL_ERR, "Failed to read a compressed file. path: \"%s\": err: \"%s\" eof: %d",
              get_filename(filename, fp),
              str_err,
              eof);
}

int fread_compressed_string(struct compressed_string *compressed, FILE *fp) {
    const ssize_t file_size = get_file_size(fp);
    char *buf;
    size_t read_bytes;
    int err = 0;

    if (file_size < 0) {
        return (int)file_size;
    }

    buf = selva_malloc(file_size);

    read_bytes = fread(buf, sizeof(char), file_size, fp);
    if (read_bytes != (size_t)file_size) {
        print_read_error(fp);
        err = SELVA_EINVAL;
        goto fail;
    }

    /* TODO Avoid double alloc */
    memcpy(&compressed->uncompressed_size, buf, sizeof(compressed->uncompressed_size));
    compressed->s = selva_string_create(buf + sizeof(compressed->uncompressed_size), file_size - sizeof(compressed->uncompressed_size), 0);

fail:
    selva_free(buf);
    return err;
}

void RDBSaveCompressedString(struct selva_io *io, struct compressed_string *compressed) {
    selva_io_save_signed(io, compressed->uncompressed_size);
    selva_io_save_string(io, compressed->s);
}

void RDBLoadCompressedString(struct selva_io *io, struct compressed_string *compressed) {
    compressed->uncompressed_size = selva_io_load_signed(io);
    compressed->s = selva_io_load_string(io);
}

static int init_compressor(void) {
    compressor = libdeflate_alloc_compressor(selva_glob_config.hierarchy_compression_level);
    if (!compressor) {
        return SELVA_EGENERAL;
    }

    decompressor = libdeflate_alloc_decompressor();
    if (!decompressor) {
        return SELVA_EGENERAL;
    }

    return 0;
}
SELVA_ONLOAD(init_compressor);

static void deinit_compressor(void) {
    libdeflate_free_compressor(compressor);
    compressor = NULL;

    libdeflate_free_decompressor(decompressor);
    decompressor = NULL;
}
SELVA_ONUNLOAD(deinit_compressor);
