#include "redismodule.h"
#include "errors.h"

const char * const selvaStrError[-SELVA_INVALID_ERROR + 1];

int replyWithSelvaError(RedisModuleCtx *ctx __unused, int err __unused) {
    return 0;
}
