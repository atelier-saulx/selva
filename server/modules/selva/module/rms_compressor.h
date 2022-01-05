#pragma once
#ifndef _RMS_COMPRESSOR_H_
#define _RMS_COMPRESSOR_H_

struct RedisModuleString;

/**
 * Compressed RedisModuleString.
 */
struct compressed_rms {
    ssize_t uncompressed_size; /*!< if equal or greater than zero then rms is compressed; Otherwise rms is uncompressed. */
    struct RedisModuleString *rms;
};

/**
 * Allocate a compressed_rms structure.
 * @returns a new compressed_rms structure.
 */
__attribute__ ((malloc)) static inline struct compressed_rms *rms_alloc_compressed(void) {
    return RedisModule_Calloc(1, sizeof(struct compressed_rms));
}

/**
 * Free a compressed RedisModuleString.
 */
static inline void rms_free_compressed(struct compressed_rms *compressed_rms) {
    if (compressed_rms->rms) {
        RedisModule_FreeString(NULL, compressed_rms->rms);
    }
    RedisModule_Free(compressed_rms);
}

/**
 * Compress RedisModuleString `in` to `out`.
 * @returns a Selva error code is returned.
 */
int rms_compress(struct compressed_rms *out, RedisModuleString *in);

/**
 * Decompress `in` and return it as `out`.
 * @returns a Selva error code is returned.
 */
int rms_decompress(struct RedisModuleString **out, struct compressed_rms *in);

#endif /* _RMS_COMPRESSOR_H_ */
