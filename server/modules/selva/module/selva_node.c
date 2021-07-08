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

    err = SelvaObject_SetStringStr(obj, SELVA_ID_FIELD, sizeof(SELVA_ID_FIELD) - 1, key_name);
    if (err) {
        return err;
    }
    RedisModule_RetainString(ctx, key_name);

    /* Set the type for root. */
    if (is_root) {
        RedisModuleString *type;

        type = RedisModule_CreateStringPrintf(NULL, "root");
        if (unlikely(!type)) {
            return SELVA_ENOMEM;
        }

        err = SelvaObject_SetStringStr(obj, "type", 4, type);
        if (err) {
            /* We leave the object as is even though it's missing type */
            fprintf(stderr, "%s:%d: Failed to set type field for root\n",
                    __FILE__, __LINE__);
            return err;
        }
        RedisModule_RetainString(ctx, type);
    }

    return 0;
}

static void delete_node_aliases(RedisModuleCtx *ctx, struct SelvaObject *obj) {
    struct SelvaSet *node_aliases_set;

    node_aliases_set = SelvaObject_GetSetStr(obj, SELVA_ALIASES_FIELD, sizeof(SELVA_ALIASES_FIELD) - 1);
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

        delete_node_aliases(ctx, obj);
        RedisModule_DeleteKey(key);
        RedisModule_CloseKey(key);
    }

    return 0;
}

int SelvaNode_ClearFields(RedisModuleCtx *ctx, struct SelvaObject *obj) {
    RedisModuleString *id;
    long long createdAt;
    int createdAtSet;
    int err;

    /* Preserve the id string. */
    err = SelvaObject_GetStringStr(obj, SELVA_ID_FIELD, sizeof(SELVA_ID_FIELD) - 1, &id);
    if (err) {
        return err;
    }
    RedisModule_RetainString(ctx, id);

    /* Preserve the createdAt field value. */
    createdAtSet = !SelvaObject_GetLongLongStr(obj, SELVA_CREATED_AT_FIELD, sizeof(SELVA_CREATED_AT_FIELD) - 1, &createdAt);

    /* Clear all the keys in the object. */
    SelvaObject_Clear(obj);

    /* Restore the id. */
    err = SelvaObject_SetStringStr(obj, SELVA_ID_FIELD, sizeof(SELVA_ID_FIELD) - 1, id);
    if (err) {
        return err;
    }

    /* Restore createdAt if it was set before this op. */
    if (createdAtSet) {
        err = SelvaObject_SetLongLongStr(obj, SELVA_CREATED_AT_FIELD, sizeof(SELVA_CREATED_AT_FIELD) - 1, createdAt);
        if (err) {
            return err;
        }
    }

    return 0;
}
