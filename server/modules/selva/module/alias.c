#include "cdefs.h"
#include "redismodule.h"
#include "errors.h"
#include "selva.h"
#include "selva_object.h"
#include "selva_set.h"
#include "hierarchy.h"
#include "alias.h"

RedisModuleKey *open_aliases_key(RedisModuleCtx *ctx) {
    RedisModuleKey * key;
    RedisModuleString *alias_key_name;

    alias_key_name = RedisModule_CreateString(ctx, SELVA_ALIASES_KEY, sizeof(SELVA_ALIASES_KEY) - 1);
    if (!alias_key_name) {
        fprintf(stderr, "%s:%d: ENOMEM\n", __FILE__, __LINE__);
        return NULL;
    }

    key = RedisModule_OpenKey(ctx, alias_key_name, REDISMODULE_READ | REDISMODULE_WRITE);

    return key;
}

int delete_aliases(RedisModuleKey *aliases_key, struct SelvaSet *set) {
    struct SelvaSetElement *el;

    if (!set || set->type != SELVA_SET_TYPE_RMSTRING) {
        /* Likely there were no aliases. */
        return SELVA_ENOENT;
    }

    SELVA_SET_RMS_FOREACH(el, set) {
        RedisModuleString *alias = el->value_rms;

        RedisModule_HashSet(aliases_key, REDISMODULE_HASH_NONE, alias, REDISMODULE_HASH_DELETE, NULL);
    }

    return 0;
}

/*
 * Caller must set the alias to the new node.
 */
void update_alias(SelvaModify_Hierarchy *hierarchy, RedisModuleKey *alias_key, RedisModuleString *id, RedisModuleString *ref) {
    RedisModuleString *orig = NULL;

    /*
     * Remove the alias from the previous node.
     */
    if (!RedisModule_HashGet(alias_key, REDISMODULE_HASH_NONE, ref, &orig, NULL)) {
        if (orig) {
            TO_STR(orig);
            Selva_NodeId node_id;
            struct SelvaModify_HierarchyNode *node;

            Selva_NodeIdCpy(node_id, orig_str);
            node = SelvaHierarchy_FindNode(hierarchy, node_id);
            if (node) {
                struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);

                SelvaObject_RemStringSetStr(obj, SELVA_ALIASES_FIELD, sizeof(SELVA_ALIASES_FIELD) - 1, ref);
            }
        }
    }

    RedisModule_HashSet(alias_key, REDISMODULE_HASH_NONE, ref, id, NULL);
}
