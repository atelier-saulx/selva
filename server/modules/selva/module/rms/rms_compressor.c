#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libdeflate.h"
#include "redismodule.h"
#include "jemalloc.h"
#include "cdefs.h"
#include "config.h"
#include "selva_onload.h"
#include "auto_free.h"
#include "errors.h"
#include "rms.h"

static struct libdeflate_compressor *compressor;
static struct libdeflate_decompressor *decompressor;

int rms_compress(struct compressed_rms *out, RedisModuleString *in, double *cratio) {
    char *compressed_str __selva_autofree = NULL;
    size_t compressed_size = 0;
    TO_STR(in);

    compressed_str = selva_malloc(in_len);
    compressed_size = libdeflate_deflate_compress(compressor, in_str, in_len, compressed_str, in_len);

    if (compressed_size == 0) {
        /*
         * No compression was achieved.
         * Therefore we use the original uncompressed string.
         * */
        compressed_size = in_len;
        out->uncompressed_size = -1;
        out->rms = RedisModule_HoldString(NULL, in);
    } else {
        /*
         * The string was compressed.
         */
        out->uncompressed_size = in_len;
        out->rms = RedisModule_CreateString(NULL, compressed_str, compressed_size);
    }

    if (cratio) {
        *cratio = (double)in_len / (double)compressed_size;
    }

    return 0;
}

int rms_decompress(RedisModuleString **out, struct compressed_rms *in) {
    size_t compressed_len;
    const char *compressed_str;
    char *uncompressed_str __selva_autofree = NULL;
    size_t nbytes_out = 0;
    enum libdeflate_result res;

    if (in->uncompressed_size < 0) {
        *out = RedisModule_HoldString(NULL, in->rms);
        return (*out) ? 0 : SELVA_ENOMEM;
    }

    uncompressed_str = selva_malloc(in->uncompressed_size);
    compressed_str = RedisModule_StringPtrLen(in->rms, &compressed_len);
    res = libdeflate_deflate_decompress(decompressor, compressed_str, compressed_len, uncompressed_str, in->uncompressed_size, &nbytes_out);
    if (res != 0 || nbytes_out != (size_t)in->uncompressed_size) {
        return SELVA_EINVAL;
    }

    *out = RedisModule_CreateString(NULL, uncompressed_str, in->uncompressed_size);
    return 0;
}

int rms_fwrite_compressed(const struct compressed_rms *compressed, FILE *fp) {
    size_t buf_size;
    const char *buf = RedisModule_StringPtrLen(compressed->rms, &buf_size);
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

    fprintf(stderr, "%s:%d: Failed to read a compressed subtree file. path: \"%s\": err: \"%s\" eof: %d\n",
            __FILE__, __LINE__,
            get_filename(filename, fp),
            str_err,
            eof);
}

int rms_fread_compressed(struct compressed_rms *compressed, FILE *fp) {
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

    memcpy(&compressed->uncompressed_size, buf, sizeof(compressed->uncompressed_size));
    compressed->rms = RedisModule_CreateString(NULL, buf + sizeof(compressed->uncompressed_size), file_size - sizeof(compressed->uncompressed_size));

fail:
    selva_free(buf);
    return err;
}

void rms_RDBSaveCompressed(RedisModuleIO *io, struct compressed_rms *compressed) {
    RedisModule_SaveSigned(io, compressed->uncompressed_size);
    RedisModule_SaveString(io, compressed->rms);
}

void rms_RDBLoadCompressed(RedisModuleIO *io, struct compressed_rms *compressed) {
    compressed->uncompressed_size = RedisModule_LoadSigned(io);
    compressed->rms = RedisModule_LoadString(io);
}

static int init_compressor(void) {
    compressor = libdeflate_alloc_compressor(selva_glob_config.hierarchy_compression_level);
    if (!compressor) {
        return REDISMODULE_ERR;
    }

    decompressor = libdeflate_alloc_decompressor();
    if (!decompressor) {
        return REDISMODULE_ERR;
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
