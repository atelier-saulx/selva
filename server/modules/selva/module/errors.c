#include "redismodule.h"
#include "linker_set.h"
#include "errors.h"

const char * const hierarchyStrError[] = {
    (const char *)"ERR_SELVA No Error",
    (const char *)"ERR_HIERARCHY EGENERAL Unknown error",
    (const char *)"ERR_HIERARCHY ENOTSUP Operation not supported",
    (const char *)"ERR_HIERARCHY EINVAL Invalid argument or input value",
    (const char *)"ERR_HIERARCHY ENOMEM Out of memory",
    (const char *)"ERR_HIERARCHY ENOENT Not found",
    (const char *)"ERR_HIERARCHY EEXIST Exist",
    (const char *)"ERR_SUBSCRIPTIONS EGENERAL Unknown error",
    (const char *)"ERR_SUBSCRIPTIONS EINVAL Invalid argument or input value",
    (const char *)"ERR_SUBSCRIPTIONS ENOMEM Out of memory",
    (const char *)"ERR_SUBSCRIPTIONS ENOENT Not found",
    (const char *)"ERR_SUBSCRIPTIONS EEXIST Exist",
    (const char *)"ERR_SELVA Invalid error code"
};

int replyWithSelvaError(RedisModuleCtx *ctx, int err) {
    if (err >= 0 || -err >= (int)num_elem(hierarchyStrError)) {
        return RedisModule_ReplyWithError(ctx, hierarchyStrError[-SELVA_EGENERAL]);
    }
    return RedisModule_ReplyWithError(ctx, hierarchyStrError[-err]);
}

