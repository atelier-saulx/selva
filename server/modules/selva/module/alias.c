#include "cdefs.h"
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

    return 0;
}

void update_alias(RedisModuleCtx *ctx, RedisModuleKey *alias_key, RedisModuleString *id, RedisModuleString *ref) {
    RedisModuleString *orig;

    /*
     * Remove the alias from the previous "ID.aliases" zset.
     */
    if (!RedisModule_HashGet(alias_key, REDISMODULE_HASH_NONE, ref, &orig, NULL)) {
        TO_STR(orig);
        RedisModuleString *key_name;
        RedisModuleKey *key;

        key_name = RedisModule_CreateStringPrintf(ctx, "%.*s%s", orig_len, orig_str, ".aliases");
        key = RedisModule_OpenKey(ctx, key_name, REDISMODULE_READ | REDISMODULE_WRITE);
        if (key) {
            RedisModule_ZsetRem(key, ref, NULL);
        }

         RedisModule_CloseKey(key);
    }

    RedisModule_HashSet(alias_key, REDISMODULE_HASH_NONE, ref, id, NULL);
}
