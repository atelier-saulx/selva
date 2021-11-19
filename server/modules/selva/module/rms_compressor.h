#pragma once
#ifndef _RMS_COMPRESSOR_H_
#define _RMS_COMPRESSOR_H_

struct RedisModuleString;

struct compressed_rms {
    ssize_t uncompressed_size; /*!< if equal or greater than zero then rms is compressed; Otherwise rms is uncompressed. */
    struct RedisModuleString *rms;
};

static inline struct compressed_rms *rms_alloc_compressed(void) {
    return RedisModule_Calloc(1, sizeof(struct compressed_rms));
}

static inline void rms_free_compressed(struct compressed_rms *compressed_rms) {
    if (compressed_rms->rms) {
        RedisModule_FreeString(NULL, compressed_rms->rms);
    }
    RedisModule_Free(compressed_rms);
}

int rms_compress(struct compressed_rms *out, RedisModuleString *in);
int rms_decompress(struct RedisModuleString **out, struct compressed_rms *in);

#endif /* _RMS_COMPRESSOR_H_ */
