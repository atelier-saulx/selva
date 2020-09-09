#include "redismodule.h"
#include "errors.h"

const char * const selvaStrError[-SELVA_INVALID_ERROR + 1];

int replyWithSelvaError(RedisModuleCtx *ctx, int err) {
    return 0;
}
