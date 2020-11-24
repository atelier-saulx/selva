#include <stddef.h>
#include "redismodule.h"
#include "alias.h"
#include "errors.h"
#include "hierarchy.h"
#include "selva_node.h"
#include "selva_object.h"
#include "selva_set.h"

int SelvaNode_Initialize(RedisModuleCtx *ctx, RedisModuleKey *key, RedisModuleString *key_name, const Selva_NodeId nodeId) {
    const int is_root = !memcmp(nodeId, ROOT_NODE_ID, SELVA_NODE_ID_SIZE);
    struct SelvaObject *obj;
    int err;

    err = SelvaObject_Key2Obj(key, &obj);
    if (err) {
        return err;
    }

    SelvaObject_SetStr(obj, RedisModule_CreateString(ctx, "$id", 3), key_name);

    /* Set the type for root. */
    if (is_root) {
        RedisModuleString *type;

        type = RedisModule_CreateStringPrintf(ctx, "root");
        if (unlikely(!type)) {
            return SELVA_MODIFY_HIERARCHY_ENOMEM;
        }

        SelvaObject_SetStr(obj, RedisModule_CreateString(ctx, "type", 4), type);
    }

    return 0;
}

static void delete_node_aliases(RedisModuleCtx *ctx, struct SelvaObject *obj, RedisModuleString *id) {
    struct SelvaSet *node_aliases_set;
    RedisModuleString *afield_name;

    afield_name = RedisModule_CreateStringPrintf(ctx, "aliases");
    if (unlikely(!afield_name)) {
        TO_STR(id);

        fprintf(stderr, "%s: OOM; Unable to remove aliases of the node: \"%s\"", __FILE__, id_str);
    }

    node_aliases_set = SelvaObject_GetSet(obj, afield_name);
    if (node_aliases_set) {
        RedisModuleKey *aliases_key;

        aliases_key = open_aliases_key(ctx);
        if (aliases_key) {
            delete_aliases(aliases_key, node_aliases_set);
            RedisModule_CloseKey(aliases_key);
        } else {
            fprintf(stderr, "%s: Unable to open aliases\n", __FILE__);
        }
    }
}

int SelvaNode_Delete(RedisModuleCtx *ctx, RedisModuleString *id) {
    RedisModuleKey *key;

    key = RedisModule_OpenKey(ctx, id, REDISMODULE_WRITE);
    if (key) {
        struct SelvaObject *obj;
        int err;

        err = SelvaObject_Key2Obj(key, &obj);
        if (err) {
            return err;
        }

        delete_node_aliases(ctx, obj, id);
        RedisModule_DeleteKey(key);
        RedisModule_CloseKey(key);
    }

    return 0;
}
