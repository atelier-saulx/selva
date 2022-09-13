#pragma once
#ifndef _SELVA_RMS_H_
#define _SELVA_RMS_H_

struct RedisModuleIO;
struct RedisModuleString;

/**
 * @addtogroup RMS
 * @{
 */

/**
 * @addtogroup shared
 * @{
 */

/**
 * Share a RedisModuleString.
 * Anything passed to this function is expected to be needed forever read-only
 * and there is no way to free anything nor determine if something could be
 * freed. This storage model is ideal for things like type strings that are
 * unlikely to ever change or change extremely rarely but are still used
 * everywhere.
 *
 * @returns If rms is not shared yet it will be added to the internal data structure;
 *          If rms is shared a pointer to the previously shared RedisModuleString is returned;
 *          A NULL pointer is returned if adding rms to the internal data structure fails.
 */
struct RedisModuleString *Share_RMS(const char *key_str, size_t key_len, struct RedisModuleString *rms);

/**
 * @}
 */

/**
 * @addtogroup compressor
 * @{
 */

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
    return selva_calloc(1, sizeof(struct compressed_rms));
}

/**
 * Free a compressed RedisModuleString.
 */
static inline void rms_free_compressed(struct compressed_rms *compressed_rms) {
    if (compressed_rms->rms) {
        RedisModule_FreeString(NULL, compressed_rms->rms);
    }
    selva_free(compressed_rms);
}

/**
 * Compress RedisModuleString `in` to `out`.
 * @returns a Selva error code is returned.
 */
int rms_compress(struct compressed_rms *out, RedisModuleString *in, double *cratio);

/**
 * Decompress `in` and return it as `out`.
 * @returns a Selva error code is returned.
 */
int rms_decompress(struct RedisModuleString **out, struct compressed_rms *in);

/**
 * Write a compressed_rms to fp.
 * Note that the binary format is not portable and it's only assumed to be read
 * by the same process (on the same machine) that wrote it.
 */
int rms_fwrite_compressed(const struct compressed_rms *compressed, FILE *fp);

/**
 * Read a compressed_rms from fp.
 * Note that the binary format is not portable and it's only assumed to be read
 * by the same process (on the same machine) that wrote it.
 */
int rms_fread_compressed(struct compressed_rms *compressed, FILE *fp);

/**
 * RDB save a compressed string.
 */
void rms_RDBSaveCompressed(struct RedisModuleIO *io, struct compressed_rms *compressed);

/**
 * RDB load a compressed string.
 */
void rms_RDBLoadCompressed(struct RedisModuleIO *io, struct compressed_rms *compressed);

/**
 * @}
 */

/**
 * @}
 */

#endif /* _SELVA_RMS_H_ */
