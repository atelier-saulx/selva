#include <stddef.h>
#include "redismodule.h"
#include "alias.h"
#include "errors.h"
#include "hierarchy.h"
#include "selva_onload.h"

int SelvaResolve_NodeId(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        RedisModuleString **ids,
        size_t nr_ids,
        Selva_NodeId node_id) {
    RedisModuleKey *aliases_key;
    RedisModuleKey *key;
    int res = SELVA_ENOENT;

    if (nr_ids == 0) {
        memcpy(node_id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE);

        return 0;
    }

    aliases_key = open_aliases_key(ctx);
    if (!aliases_key) {
        return SELVA_EGENERAL;
    }

    for (size_t i = 0; i < nr_ids; i++) {
        RedisModuleString *id = ids[i];
        TO_STR(id);

        /* First check if it's a nodeId. */
        if (id_len <= SELVA_NODE_ID_SIZE) {
            Selva_NodeIdCpy(node_id, id_str);

            /* We assume that root always exists. */
            if (!memcmp(node_id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE)) {
                res = 0;
                break;
            }

            if (SelvaModify_HierarchyNodeExists(hierarchy, node_id)) {
                res = 0;
                break;
            }
        }

        /* Then check if there is an alias with this string. */
        RedisModuleString *orig = NULL;
        if (!RedisModule_HashGet(aliases_key, REDISMODULE_HASH_NONE, id, &orig, NULL)) {
            if (orig) {
                TO_STR(orig);

                Selva_NodeIdCpy(node_id, orig_str);
                if (SelvaModify_HierarchyNodeExists(hierarchy, node_id)) {
                    res = 0;
                    break;
                }
            }
        }
    }

    RedisModule_CloseKey(aliases_key);
    return res;
}

int SelvaResolve_NodeIdCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const size_t ARGV_KEY = 1;
    const size_t ARGV_OKEY = 2;

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    /* TODO */
    return replyWithSelvaErrorf(ctx, SELVA_EGENERAL, "NOT IMPL");

    return REDISMODULE_OK;
}

static int SelvaResolve_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.resolve.nodeid", SelvaResolve_NodeIdCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(SelvaResolve_OnLoad);
