#include "redismodule.h"

RedisModuleKey *open_aliases_key(RedisModuleCtx *ctx) {
    RedisModuleKey * key;

    RedisModuleString *alias_key_name = RedisModule_CreateStringPrintf(ctx, "___selva_aliases");
    key = RedisModule_OpenKey(ctx, alias_key_name, REDISMODULE_READ | REDISMODULE_WRITE);

    return key;
}
