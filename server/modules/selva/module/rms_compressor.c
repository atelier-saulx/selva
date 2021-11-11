#include <stddef.h>
#include "libdeflate.h"
#include "redismodule.h"
#include "cdefs.h"
#include "auto_free.h"
#include "errors.h"
#include "rms_compressor.h"

int rms_compress(struct compressed_rms *out, RedisModuleString *in) {
    struct libdeflate_compressor *compressor;
    char *compressed_str __auto_free = NULL;
    size_t compressed_size = 0;
    TO_STR(in);

    compressor = libdeflate_alloc_compressor(6);
    if (!compressor) {
        return SELVA_ENOMEM;
    }

    compressed_str = RedisModule_Alloc(in_len);
    if (compressed_str) {
        /*
         * If mem allocation should fail we can just return the string we
         * already have and we can just skip the compression phase.
         */
        compressed_size = libdeflate_deflate_compress(compressor, in_str, in_len, compressed_str, in_len);
        libdeflate_free_compressor(compressor);
    }

    if (compressed_size == 0) {
        /* No compression was achieved. */
        compressed_size = in_len;
        out->uncompressed_size = -1;
        out->rms = RedisModule_HoldString(NULL, in);
    } else {
        out->uncompressed_size = in_len;
        out->rms = RedisModule_CreateString(NULL, compressed_str, compressed_size);
    }

    fprintf(stderr, "%s:%d: Compression ratio: %.2f:1\n",
            __FILE__, __LINE__,
            (double)in_len / (double)compressed_size);

    return 0;
}

int rms_decompress(RedisModuleString **out, struct compressed_rms *in) {
    size_t compressed_len;
    const char *compressed_str;
    char *uncompressed_str __auto_free = NULL;
    struct libdeflate_decompressor *decompressor;
    size_t nbytes_out = 0;
    enum libdeflate_result res;
    RedisModuleString *raw;

    if (in->uncompressed_size < 0) {
        *out = RedisModule_HoldString(NULL, in->rms);
        return (*out) ? 0 : SELVA_ENOMEM;
    }

    uncompressed_str = RedisModule_Alloc(in->uncompressed_size);
    if (!uncompressed_str) {
        return SELVA_ENOMEM;
    }

    decompressor = libdeflate_alloc_decompressor();
    if (!decompressor) {
        return SELVA_ENOMEM;
    }

    compressed_str = RedisModule_StringPtrLen(in->rms, &compressed_len);
    res = libdeflate_deflate_decompress(decompressor, compressed_str, compressed_len, uncompressed_str, in->uncompressed_size, &nbytes_out);
    libdeflate_free_decompressor(decompressor);
    if (res != 0 || nbytes_out != (size_t)in->uncompressed_size) {
        return SELVA_EINVAL;
    }

    raw = RedisModule_CreateString(NULL, uncompressed_str, in->uncompressed_size);
    if (!raw) {
        return SELVA_ENOMEM;
    }

    *out = raw;
    return 0;
}
