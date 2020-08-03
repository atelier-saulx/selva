#include "redismodule.h"

RedisModuleKey *open_aliases_key(RedisModuleCtx *ctx) {
    RedisModuleKey * key;

    RedisModuleString *alias_key_name = RedisModule_CreateStringPrintf(ctx, "___selva_aliases");
    key = RedisModule_OpenKey(ctx, alias_key_name, REDISMODULE_READ | REDISMODULE_WRITE);

    return key;
}

int delete_aliases(RedisModuleKey *aliases_key, RedisModuleKey *set_key) {
    if (RedisModule_ZsetFirstInScoreRange(set_key, REDISMODULE_NEGATIVE_INFINITE, REDISMODULE_POSITIVE_INFINITE, 0, 0) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    while (!RedisModule_ZsetRangeEndReached(set_key)) {
        RedisModuleString *alias;

        alias = RedisModule_ZsetRangeCurrentElement(set_key, NULL);
        RedisModule_HashSet(aliases_key, REDISMODULE_HASH_NONE, alias, REDISMODULE_HASH_DELETE, NULL);
        RedisModule_ZsetRangeNext(set_key);
    }

    RedisModule_ZsetRangeStop(set_key);
}
