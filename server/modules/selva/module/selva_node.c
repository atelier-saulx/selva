#include <stddef.h>
#include "redismodule.h"
#include "hierarchy.h"
#include "errors.h"
#include "selva_node.h"

RedisModuleKey *SelvaNode_Open(RedisModuleCtx *ctx, SelvaModify_Hierarchy *hierarchy, RedisModuleString *id, Selva_NodeId nodeId, unsigned flags) {
    /*
     * If this is a new node we need to create a hierarchy node for it.
     */
    if (!SelvaModify_HierarchyNodeExists(hierarchy, nodeId)) {
        size_t nr_parents;
        int err;

        if ((flags & SELVA_NODE_OPEN_CREATE_FLAG) == 0) {
            return NULL;
        }

        nr_parents = unlikely(flags & SELVA_NODE_OPEN_NO_ROOT_FLAG) ? 0 : 1;
        err = SelvaModify_SetHierarchy(ctx, hierarchy, nodeId, nr_parents, ((Selva_NodeId []){ ROOT_NODE_ID }), 0, NULL);
        if (err) {
            fprintf(stderr, "%s: %s\n", __FILE__, getSelvaErrorStr(err));
            return NULL;
        }
    }

    return RedisModule_OpenKey(ctx, id, REDISMODULE_WRITE);
}

int SelvaNode_GetField(RedisModuleCtx *ctx, RedisModuleKey *node_key, RedisModuleString *field, RedisModuleString **out) {
    if (RedisModule_HashGet(node_key, REDISMODULE_HASH_NONE, field, out, NULL) != REDISMODULE_OK) {
        /* TODO Can we determine the exact cause? */
        return SELVA_EGENERAL;
    }

    return 0;
}

int SelvaNode_SetField(RedisModuleCtx *ctx, RedisModuleKey *node_key, RedisModuleString *field, RedisModuleString *value) {
    /* TODO handle objects properly */

    (void)RedisModule_HashSet(node_key, REDISMODULE_HASH_NONE, field, value, NULL);

    return 0;
}

int SelvaNode_DelField(RedisModuleCtx *ctx, RedisModuleKey *node_key, RedisModuleString *field) {
    (void)RedisModule_HashSet(node_key, REDISMODULE_HASH_NONE, field, REDISMODULE_HASH_DELETE, NULL);

    return 0;
}
