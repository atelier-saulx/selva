#include <stdlib.h>
#include "redismodule.h"
#include "rms.h"

int rms_compress(struct compressed_rms *out, RedisModuleString *in, double *cratio) {
    if (cratio) {
        *cratio = 1.0;
    }

    return 0;
}

int rms_decompress(RedisModuleString **out, struct compressed_rms *in) {
    return 0;
}

/* TODO Might want to implement these. */
void rms_RDBSaveCompressed(RedisModuleIO *io, struct compressed_rms *compressed) {
    return;
}

void rms_RDBLoadCompressed(RedisModuleIO *io, struct compressed_rms *compressed) {
    return;
}
