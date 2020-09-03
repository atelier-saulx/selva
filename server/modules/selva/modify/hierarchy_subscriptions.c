#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "selva_onload.h"
#include "async_task.h"
#include "cdefs.h"
#include "hierarchy.h"
#include "redismodule.h"
#include "rpn.h"
#include "svector.h"

struct SelvaModify_HierarchySubscription {
    Selva_SubscriptionId sub_id;
    Selva_NodeId sub_node_id;
    uint16_t _spare;
    enum Selva_SubscriptionType sub_type;
    rpn_token *sub_filter;
    struct SelvaModify_HierarchySubscription *next_sub;
};

static struct SelvaModify_HierarchySubscription *subs_head;

static int subscription_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const struct SelvaModify_HierarchySubscription *a = *(const struct SelvaModify_HierarchySubscription **)a_raw;
    const struct SelvaModify_HierarchySubscription *b = *(const struct SelvaModify_HierarchySubscription **)b_raw;

    return memcmp(a->sub_id, b->sub_id, sizeof(Selva_SubscriptionId));
}

static char *subId2str(Selva_SubscriptionId sub_id) {
    static char str[2 * sizeof(Selva_SubscriptionId)];

    for (size_t i = 0; i < sizeof(Selva_SubscriptionId); i++) {
        sprintf(str + 2 * i, "%02x", sub_id[i]);
    }
    str[sizeof(str) - 1] = '\0';

    return str;
}

static void init_subs(
        Selva_NodeId id __unused,
        struct SelvaModify_HierarchyMetaData *metadata) {
    SVector_Init(&metadata->subs, 1, subscription_compare);
}
SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(init_subs);

static void deinit_subs(
        Selva_NodeId id __unused,
        struct SelvaModify_HierarchyMetaData *metadata) {
    SVector_Destroy(&metadata->subs);
}
SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(deinit_subs);

static int setSubscriptionMarker(Selva_NodeId id, void *arg, struct SelvaModify_HierarchyMetaData *metadata) {
    struct SelvaModify_HierarchySubscription *sub;

    sub = (struct SelvaModify_HierarchySubscription *)arg;
    SVector_InsertFast(&metadata->subs, sub);

    return 0;
}

static int clearSubscriptionMarker(Selva_NodeId id, void *arg, struct SelvaModify_HierarchyMetaData *metadata) {
    struct SelvaModify_HierarchySubscription *sub;

    sub = (struct SelvaModify_HierarchySubscription *)arg;
    SVector_Remove(&metadata->subs, sub);

    return 0;
}

int SelvaModify_CreateSubscription(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_SubscriptionId sub_id,
        enum Selva_SubscriptionType type,
        Selva_NodeId node_id) {
    enum SelvaModify_HierarchyTraversal dir;
    struct SelvaModify_HierarchySubscription *sub;

    switch (type) {
    case SELVA_SUBSCRIPTION_TYPE_ANCESTORS:
        dir = SELVA_MODIFY_HIERARCHY_DFS_ANCESTORS;
        break;
    case SELVA_SUBSCRIPTION_TYPE_DESCENDANTS:
        dir = SELVA_MODIFY_HIERARCHY_DFS_DESCENDANTS;
        break;
    default:
        return SELVA_MODIFY_HIERARCHY_EINVAL;
    }

    sub = RedisModule_Alloc(sizeof(struct SelvaModify_HierarchySubscription));
    if (!sub) {
        return SELVA_MODIFY_HIERARCHY_ENOMEM;
    }

    memcpy(sub->sub_id, sub_id, sizeof(sub->sub_id));
    memcpy(sub->sub_node_id, node_id, SELVA_NODE_ID_SIZE);
    sub->sub_type = type;
    sub->sub_filter = NULL;

    /*
     * Add to the linked list of subscriptions.
     */
    sub->next_sub = subs_head;
    subs_head = sub;

    /*
     * Add subscription markers.
     */
    struct SelvaModify_HierarchyCallback cb = {
        .node_cb = setSubscriptionMarker,
        .node_arg = sub,
    };

    (void)SelvaModify_TraverseHierarchy(hierarchy, sub->sub_node_id, dir, &cb);

    return 0;
}

void SelvaModify_DeleteSubscription(struct SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id) {
    struct SelvaModify_HierarchySubscription *sub = subs_head;
    struct SelvaModify_HierarchySubscription *prev_sub;

    while (sub) {
        if (!memcmp(sub->sub_id, sub_id, sizeof(Selva_SubscriptionId))) {
            break;
        }
        prev_sub = sub;
        sub = sub->next_sub;
    }
    if (sub) {
        enum SelvaModify_HierarchyTraversal dir;

        switch (sub->sub_type) {
        case SELVA_SUBSCRIPTION_TYPE_ANCESTORS:
            dir = SELVA_MODIFY_HIERARCHY_DFS_ANCESTORS;
            break;
        case SELVA_SUBSCRIPTION_TYPE_DESCENDANTS:
            dir = SELVA_MODIFY_HIERARCHY_DFS_DESCENDANTS;
            break;
        default:
            fprintf(stderr, "Hierarchy: Subscription deletion failed: Invalid subscription %s", subId2str(sub_id));
            return;
        }

        /* Remove from the list. */
        prev_sub->next_sub = sub->next_sub;

        /*
         * Remove subscription markers.
         */
        struct SelvaModify_HierarchyCallback cb = {
            .node_cb = clearSubscriptionMarker,
            .node_arg = sub,
        };
        (void)SelvaModify_TraverseHierarchy(hierarchy, sub->sub_node_id, dir, &cb);

        RedisModule_Free(sub);
    }
}

void SelvaModify_ClearAllSubscriptionMarkers(Selva_NodeId id __unused, struct SelvaModify_HierarchyMetaData *metadata) {
    /* TODO for each dir => clear */
    SVector_Clear(&metadata->subs);
}

static int parse_subscription_id(Selva_SubscriptionId id, RedisModuleString *arg) {
    char byte[3] = { '\0', '\0', '\0' };
    TO_STR(arg);

    if (arg_len != 2 * sizeof(Selva_SubscriptionId)) {
        return SELVA_MODIFY_HIERARCHY_EINVAL;
    }

    for (size_t i = 0; i < sizeof(Selva_SubscriptionId); i++) {
        unsigned long v;

        byte[0] = arg_str[i];
        byte[1] = arg_str[i + 1];
        v = strtoul(byte, NULL, 16);

        if (unlikely(v > 0xff)) {
            return SELVA_MODIFY_HIERARCHY_EINVAL;
        }

        id[i] = v;
    }

    return 0;
}

static int parse_subscription_type(enum Selva_SubscriptionType *type, RedisModuleString *arg) {
    TO_STR(arg);

    if (!strncmp("ancestors", arg_str, arg_len)) {
        *type = SELVA_SUBSCRIPTION_TYPE_ANCESTORS;
    } else if (!strncmp("descendants", arg_str, arg_len)) {
        *type = SELVA_SUBSCRIPTION_TYPE_DESCENDANTS;
    } else {
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
    }

    return 0;
}

/*
 * KEY SUB_ID ancestors|descendants NODE_ID
 */
int SelvaModify_Hierarchy_SubscribeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const size_t ARGV_REDIS_KEY = 1;
    const size_t ARGV_SUB_ID    = 2;
    const size_t ARGV_SUB_TYPE  = 3;
    const size_t ARGV_NODE_ID   = 4;

    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    Selva_SubscriptionId sub_id;
    err = parse_subscription_id(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        fprintf(stderr, "Invalid sub_id\n");
        return replyWithHierarchyError(ctx, err);
    }

    enum Selva_SubscriptionType sub_type;
    err = parse_subscription_type(&sub_type, argv[ARGV_SUB_TYPE]);
    if (err) {
        fprintf(stderr, "Invalid type\n");
        return replyWithHierarchyError(ctx, err);
    }

    Selva_NodeId nodeId;
    size_t len;
    const char *str = RedisModule_StringPtrLen(argv[ARGV_NODE_ID], &len);
    memset(nodeId, 0, SELVA_NODE_ID_SIZE);
    memcpy(nodeId, str, (SELVA_NODE_ID_SIZE > len) ? len : SELVA_NODE_ID_SIZE);

    err = SelvaModify_CreateSubscription(hierarchy, sub_id, sub_type, nodeId);
    if (err) {
        fprintf(stderr, "failed to create a sub\n");
        return replyWithHierarchyError(ctx, err);
    }

    RedisModule_ReplyWithLongLong(ctx, 1);
    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}

/*
 * KEY [NODE_ID]
 */
int SelvaModify_Hierarchy_SubscriptionsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    return REDISMODULE_OK;
}

static int Hierarchy_Subscriptions_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.subscribe", SelvaModify_Hierarchy_SubscribeCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.subscriptions", SelvaModify_Hierarchy_SubscriptionsCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_MODIFY_ONLOAD(Hierarchy_Subscriptions_OnLoad);
