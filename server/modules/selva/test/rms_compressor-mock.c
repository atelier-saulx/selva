#include <stdlib.h>
#include "redismodule.h"
#include "rms.h"

int rms_compress(struct compressed_rms *out, RedisModuleString *in) {
    return 0;
}

int rms_decompress(RedisModuleString **out, struct compressed_rms *in) {
    return 0;
}
