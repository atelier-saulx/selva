#include <stddef.h>
#include "cdefs.h"
#include "errors.h"
#include "redismodule.h"
#include "alias.h"
#include "selva_set.h"

RedisModuleKey *SelvaSet_Open(RedisModuleCtx *ctx, const char *id_str, size_t id_len, const char *field_str) {
    RedisModuleString *set_key_name;
    RedisModuleKey *set_key;

    set_key_name = RedisModule_CreateStringPrintf(ctx, "%.*s.%s", id_len, id_str, field_str);
    if (unlikely(!set_key_name)) {
        return NULL;
    }

    set_key = RedisModule_OpenKey(ctx, set_key_name, REDISMODULE_WRITE);
    if (!set_key) {
        return NULL;
    }

    const int keytype = RedisModule_KeyType(set_key);
    if (keytype != REDISMODULE_KEYTYPE_ZSET && keytype != REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_CloseKey(set_key);
        return NULL;
    }

    return set_key;
}

int SelvaSet_Remove(RedisModuleKey *set_key, RedisModuleKey *alias_key) {
    if (!set_key) {
        return SELVA_EINVAL;
    }

    /*
     * In case of aliases we need to clear the aliases hash too.
     */
    if (alias_key) {
        delete_aliases(alias_key, set_key);
    }

    RedisModule_UnlinkKey(set_key);

    return 0;
}
