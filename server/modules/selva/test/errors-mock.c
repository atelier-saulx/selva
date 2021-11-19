#include "cdefs.h"
#include "redismodule.h"
#include "errors.h"

const char * const __attribute__((weak)) selvaStrError[-SELVA_INVALID_ERROR + 1];

int replyWithSelvaError(RedisModuleCtx *ctx, int err) {
    return 0;
}

int replyWithSelvaErrorf(RedisModuleCtx *ctx, int err, const char *fmt, ...) {
    return 0;
}
