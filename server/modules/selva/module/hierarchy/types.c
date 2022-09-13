#include "redismodule.h"
#include "jemalloc.h"
#include "selva.h"
#include "selva_onload.h"
#include "selva_object.h"
#include "rms.h"
#include "hierarchy.h"
#include "errors.h"

/**
 * This function takes care of sharing/holding name.
 */
static int SelvaHierarchyTypes_Add(RedisModuleCtx *ctx, struct SelvaHierarchy *hierarchy, const Selva_NodeType type, RedisModuleString *name) {
    struct SelvaObject *obj = SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy);
    RedisModuleString *shared;

        shared = Share_RMS(SELVA_TYPE_FIELD, sizeof(SELVA_TYPE_FIELD) - 1, name);
        if (shared) {
            return SelvaObject_SetStringStr(obj, type, SELVA_NODE_TYPE_SIZE, shared);
        } else {
            int err;

            err = SelvaObject_SetStringStr(obj, type, SELVA_NODE_TYPE_SIZE, name);
            if (err) {
                return err;
            }
            RedisModule_RetainString(ctx, name);
        }

    /* TODO Should we disallow overwrite? */
    return SelvaObject_SetStringStr(obj, type, SELVA_NODE_TYPE_SIZE, name);
}

static void SelvaHierarchyTypes_Clear(struct SelvaHierarchy *hierarchy) {
    struct SelvaObject *obj = SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy);

    SelvaObject_Clear(obj, NULL);
}

RedisModuleString *SelvaHierarchyTypes_Get(struct SelvaHierarchy *hierarchy, const Selva_NodeType type) {
    struct SelvaObject *obj = SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy);
    RedisModuleString *out = NULL;

    (void)SelvaObject_GetStringStr(obj, type, SELVA_NODE_TYPE_SIZE, &out);

    if (out) {
        RedisModule_RetainString(NULL, out);
    }

    return out;
}

int SelvaHierarchyTypes_AddCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaHierarchy *hierarchy;
    RedisModuleString *type;
    RedisModuleString *name;
    int err;

    const int ARGV_KEY = 1;
    const int ARGV_TYPE = 2;
    const int ARGV_NAME = 3;

    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    type = argv[ARGV_TYPE];
    name = argv[ARGV_NAME];
    TO_STR(type);

    if (type_len != 2) {
        return replyWithSelvaError(ctx, SELVA_EINTYPE);
    }

    err = SelvaHierarchyTypes_Add(ctx, hierarchy, type_str, name);
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    return RedisModule_ReplyWithLongLong(ctx, 1);
}

int SelvaHierarchyTypes_ClearCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaHierarchy *hierarchy;

    const int ARGV_KEY = 1;

    if (argc != SELVA_NODE_TYPE_SIZE) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    SelvaHierarchyTypes_Clear(hierarchy);
    return RedisModule_ReplyWithLongLong(ctx, 1);
}

int SelvaHierarchyTypes_ListCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaHierarchy *hierarchy;

    const int ARGV_KEY = 1;

    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    return SelvaObject_ReplyWithObject(ctx, NULL, SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy), NULL, 0);
}

static int SelvaHierarchyTypes_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.types.add", SelvaHierarchyTypes_AddCommand, "write", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.types.clear", SelvaHierarchyTypes_ClearCommand, "write", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.types.list", SelvaHierarchyTypes_ListCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(SelvaHierarchyTypes_OnLoad);
