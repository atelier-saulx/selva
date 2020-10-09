#include <stddef.h>
#include "redismodule.h"
#include "hierarchy.h"
#include "errors.h"
#include "selva_node.h"

static int initialize_node(RedisModuleCtx *ctx, RedisModuleKey *key, RedisModuleString *key_name, const Selva_NodeId nodeId) {
    const int is_root = !memcmp(nodeId, ROOT_NODE_ID, SELVA_NODE_ID_SIZE);

    RedisModule_HashSet(key, REDISMODULE_HASH_NX | REDISMODULE_HASH_CFIELDS, "$id", key_name, NULL);

    /* Set the type for root. */
    if (is_root) {
        RedisModuleString *type;

        type = RedisModule_CreateStringPrintf(ctx, "root");
        if (unlikely(!type)) {
            return SELVA_MODIFY_HIERARCHY_ENOMEM;
        }

        RedisModule_HashSet(key, REDISMODULE_HASH_NX | REDISMODULE_HASH_CFIELDS, "type", type, NULL);
    }

    return 0;
}

RedisModuleKey *SelvaNode_Open(RedisModuleCtx *ctx, SelvaModify_Hierarchy *hierarchy, RedisModuleString *id, const Selva_NodeId nodeId, unsigned flags) {
    int err;

    /*
     * If this is a new node we need to create a hierarchy node for it.
     *
     * There is dumb circular dependency here.
     * The modify command will call this function to open and create nodes.
     * However, also hierarchy will call this function to create the node.
     * It ended up like this because nodes and hierarchy are tied together so
     * closely.
     * In theory hierarchy will only call this function when the node already
     * exists but to be extra sure, hierarchy will never pass a pointer to the
     * hierarchy it's working on.
     */
    if (hierarchy && !SelvaModify_HierarchyNodeExists(hierarchy, nodeId)) {
        size_t nr_parents;

        if ((flags & SELVA_NODE_OPEN_CREATE_FLAG) == 0) {
            return NULL;
        }

        nr_parents = unlikely(flags & SELVA_NODE_OPEN_NO_ROOT_FLAG) ? 0 : 1;
        err = SelvaModify_SetHierarchy(ctx, hierarchy, nodeId, nr_parents, ((Selva_NodeId []){ ROOT_NODE_ID }), 0, NULL);
        if (err) {
            fprintf(stderr, "%s:%d key: %s err: %s\n",
                    __FILE__,
                    __LINE__,
                    RedisModule_StringPtrLen(id, NULL),
                    getSelvaErrorStr(err));
            return NULL;
        }
    }

    /*
     * Open the redis key.
     */
    const int open_mode = REDISMODULE_READ | ((flags & SELVA_NODE_OPEN_WRFLD_FLAG) ? REDISMODULE_WRITE : 0);
    RedisModuleKey *key = RedisModule_OpenKey(ctx, id, open_mode);
    if (!key) {
        fprintf(stderr, "%s:%d key: %s err: %s\n",
                __FILE__,
                __LINE__,
                RedisModule_StringPtrLen(id, NULL),
                getSelvaErrorStr(err));
        return NULL;
    }

    /*
     * If the key is empty at this point we assume that the hash should actually
     * exist regardless of the given flags. Either there is something wrong or
     * morelikely the caller is from hierarchy.
     */
    if ((flags & SELVA_NODE_OPEN_WRFLD_FLAG) && RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        err = initialize_node(ctx, key, id, nodeId);
        if (err) {
            fprintf(stderr, "%s: %s\n", __FILE__, getSelvaErrorStr(err));
            RedisModule_CloseKey(key);
            return NULL;
        }
    }

    return key;
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
