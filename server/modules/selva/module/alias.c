#include "cdefs.h"
#include "redismodule.h"
#include "selva.h"
#include "selva_object.h"
#include "selva_set.h"
#include "alias.h"

RedisModuleKey *open_aliases_key(RedisModuleCtx *ctx) {
    RedisModuleKey * key;
    RedisModuleString *alias_key_name;

    alias_key_name = RedisModule_CreateString(ctx, SELVA_ALIASES_KEY, sizeof(SELVA_ALIASES_KEY) - 1);
    if (!alias_key_name) {
        fprintf(stderr, "%s: ENOMEM\n", __FILE__);
        return NULL;
    }

    key = RedisModule_OpenKey(ctx, alias_key_name, REDISMODULE_READ | REDISMODULE_WRITE);

    return key;
}

int delete_aliases(RedisModuleKey *aliases_key, struct SelvaSet *set) {
    struct SelvaSetElement *el;

    if (!set) {
        /* Likely there were no aliases. */
        return 0;
    }

    RB_FOREACH(el, SelvaSetHead, &set->head) {
        RedisModuleString *alias = el->value;

        RedisModule_HashSet(aliases_key, REDISMODULE_HASH_NONE, alias, REDISMODULE_HASH_DELETE, NULL);
    }

    return 0;
}

void update_alias(RedisModuleCtx *ctx, RedisModuleKey *alias_key, RedisModuleString *id, RedisModuleString *ref) {
    RedisModuleString *orig = NULL;

    /*
     * Remove the alias from the previous node.
     */
    if (!RedisModule_HashGet(alias_key, REDISMODULE_HASH_NONE, ref, &orig, NULL)) {
        RedisModuleKey *key;

        if (orig) {
            /* TODO Better error handling */
            key = RedisModule_OpenKey(ctx, orig, REDISMODULE_READ | REDISMODULE_WRITE);
            if (key) {
                struct SelvaObject *obj;

                if (SelvaObject_Key2Obj(key, &obj) == 0) {
                    RedisModuleString *field;

                    field = RedisModule_CreateStringPrintf(ctx, "%s", "aliases");
                    if (field) {
                        SelvaObject_RemSet(obj, field, ref);
                    }
                }
            }

            RedisModule_CloseKey(key);
        }
    }

    RedisModule_HashSet(alias_key, REDISMODULE_HASH_NONE, ref, id, NULL);
}
