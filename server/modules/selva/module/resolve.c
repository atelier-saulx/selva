#include <stddef.h>
#include "redismodule.h"
#include "alias.h"
#include "arg_parser.h"
#include "errors.h"
#include "hierarchy.h"
#include "selva_onload.h"
#include "subscriptions.h"
#include "resolve.h"

int SelvaResolve_NodeId(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        RedisModuleString **ids,
        size_t nr_ids,
        Selva_NodeId node_id) {
    RedisModuleKey *aliases_key;
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
        TO_STR(id)

        /* First check if it's a nodeId. */
        if (id_len <= SELVA_NODE_ID_SIZE) {
            Selva_NodeIdCpy(node_id, id_str);

            /* We assume that root always exists. */
            if (!memcmp(node_id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE)) {
                res = SELVA_RESOLVE_NODE_ID;
                break;
            }

            if (SelvaModify_HierarchyNodeExists(hierarchy, node_id)) {
                res = SELVA_RESOLVE_NODE_ID;
                break;
            }
        }

        /* Then check if there is an alias with this string. */
        RedisModuleString *orig = NULL;
        if (!RedisModule_HashGet(aliases_key, REDISMODULE_HASH_NONE, id, &orig, NULL)) {
            if (orig) {
                TO_STR(orig)

                Selva_NodeIdCpy(node_id, orig_str);
                if (SelvaModify_HierarchyNodeExists(hierarchy, node_id)) {
                    res = SELVA_RESOLVE_ALIAS;
                    break;
                }
            }
        }
    }

    RedisModule_CloseKey(aliases_key);
    return res;
}

static Selva_SubscriptionMarkerId gen_marker_id(const char *s) {
    /* fnv32 */
    unsigned hash = 2166136261u;

    for (; *s; s++) {
        hash = (hash ^ *s) * 0x01000193;
    }

    return (Selva_SubscriptionMarkerId)(0x80000000 | hash);
}

/*
 * HIERARCHY_KEY SUB_ID IDS...
 */
int SelvaResolve_NodeIdCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const size_t ARGV_REDIS_KEY = 1;
    const size_t ARGV_SUB_ID = 2;
    const size_t ARGV_IDS = 3;

    if (argc < (int)ARGV_IDS + 1) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return replyWithSelvaErrorf(ctx, SELVA_MODIFY_HIERARCHY_ENOENT, "Hierarchy not found");
    }

    Selva_NodeId node_id;
    const int resolved = SelvaResolve_NodeId(ctx, hierarchy, argv + ARGV_IDS, argc - ARGV_IDS, node_id);
    if (resolved == SELVA_ENOENT) {
        return RedisModule_ReplyWithNull(ctx);
    } else if (resolved < 0) {
        return replyWithSelvaErrorf(ctx, resolved, "Resolve failed");
    }

    RedisModuleString *argv_sub_id = argv[ARGV_SUB_ID];
    TO_STR(argv_sub_id)

    if ((resolved & SELVA_RESOLVE_ALIAS) && argv_sub_id_len > 0) {
        RedisModuleString *alias_name = argv[ARGV_IDS + (resolved & ~SELVA_RESOLVE_FLAGS)];
        Selva_SubscriptionId sub_id;
        const Selva_SubscriptionMarkerId marker_id = gen_marker_id(RedisModule_StringPtrLen(alias_name, NULL));

        err = SelvaArgParser_SubscriptionId(sub_id, argv_sub_id);
        if (err) {
            return replyWithSelvaErrorf(ctx, err, "Invalid sub_id \"%s\"\n", argv_sub_id_str);
        }

        err = Selva_AddSubscriptionAliasMarker(hierarchy, sub_id, marker_id, alias_name, node_id);
        if (err) {
            return replyWithSelvaErrorf(ctx, err, "Failed to subscribe sub_id: \"%s.%d\" alias_name: %s node_id: %.*s\n",
                    argv_sub_id_str,
                    (int)marker_id,
                    RedisModule_StringPtrLen(alias_name, NULL),
                    (int)SELVA_NODE_ID_SIZE, node_id);
        }
    }

    RedisModule_ReplyWithStringBuffer(ctx, node_id, Selva_NodeIdLen(node_id));

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
