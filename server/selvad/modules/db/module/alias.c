/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include "selva.h"
#include "selva_object.h"
#include "selva_set.h"
#include "hierarchy.h"
#include "alias.h"

RedisModuleKey *open_aliases_key(RedisModuleCtx *ctx) {
    RedisModuleKey * key;
    RedisModuleString *alias_key_name;

    alias_key_name = RedisModule_CreateString(ctx, SELVA_ALIASES_KEY, sizeof(SELVA_ALIASES_KEY) - 1);
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

void update_alias(SelvaHierarchy *hierarchy, RedisModuleKey *alias_key, const Selva_NodeId node_id, RedisModuleString *ref) {
    RedisModuleString *old = NULL;

    /*
     * Remove the alias from the previous node.
     */
    if (!RedisModule_HashGet(alias_key, REDISMODULE_HASH_NONE, ref, &old, NULL)) {
        if (old) {
            TO_STR(old);
            Selva_NodeId old_node_id;
            const struct SelvaHierarchyNode *old_node;

            Selva_NodeIdCpy(old_node_id, old_str);
            old_node = SelvaHierarchy_FindNode(hierarchy, old_node_id);
            if (old_node) {
                struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(old_node);

                SelvaObject_RemStringSetStr(obj, SELVA_ALIASES_FIELD, sizeof(SELVA_ALIASES_FIELD) - 1, ref);
            }
        }
    }

    RedisModule_HashSet(alias_key, REDISMODULE_HASH_NONE, ref, RedisModule_CreateString(ctx, node_id, Selva_NodeIdLen(node_id)), NULL);
}
