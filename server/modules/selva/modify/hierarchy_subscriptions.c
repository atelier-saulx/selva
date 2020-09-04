#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#define _SELVA_MODIFY_HIERARCHY_INTERNAL_
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
    RB_ENTRY(SelvaModify_HierarchySubscription) _sub_index_entry;
};

static int subscription_svector_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const struct SelvaModify_HierarchySubscription *a = *(const struct SelvaModify_HierarchySubscription **)a_raw;
    const struct SelvaModify_HierarchySubscription *b = *(const struct SelvaModify_HierarchySubscription **)b_raw;

    return memcmp(a->sub_id, b->sub_id, sizeof(Selva_SubscriptionId));
}

static int subscription_rb_compare(const struct SelvaModify_HierarchySubscription *a, const struct SelvaModify_HierarchySubscription *b) {
    return memcmp(a->sub_id, b->sub_id, sizeof(Selva_SubscriptionId));
}

RB_PROTOTYPE_STATIC(hierarchy_subscriptions_tree, SelvaModify_HierarchySubscription, _sub_index_entry, subscription_rb_compare)
RB_GENERATE_STATIC(hierarchy_subscriptions_tree, SelvaModify_HierarchySubscription, _sub_index_entry, subscription_rb_compare)

static char *subId2str(Selva_SubscriptionId sub_id) {
    static char str[2 * sizeof(Selva_SubscriptionId) + 1];

    for (size_t i = 0; i < sizeof(Selva_SubscriptionId); i++) {
        sprintf(str + 2 * i, "%02x", sub_id[i]);
    }
    str[sizeof(str) - 1] = '\0';

    return str;
}

void SelvaModify_DestroySubscriptions(struct hierarchy_subscriptions_tree *subs_head) {
    struct SelvaModify_HierarchySubscription *sub;
    struct SelvaModify_HierarchySubscription *next;

	for (sub = RB_MIN(hierarchy_subscriptions_tree, subs_head); sub != NULL; sub = next) {
		next = RB_NEXT(hierarchy_subscriptions_tree, subs_head, sub);
		RB_REMOVE(hierarchy_subscriptions_tree, subs_head, sub);
        RedisModule_Free(sub);
    }
}

static void init_node_metadata_subs(
        Selva_NodeId id __unused,
        struct SelvaModify_HierarchyMetaData *metadata) {
    SVector_Init(&metadata->subs, 1, subscription_svector_compare);
}
SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(init_node_metadata_subs);

static void deinit_node_metadata_subs(
        Selva_NodeId id __unused,
        struct SelvaModify_HierarchyMetaData *metadata) {
    SVector_Destroy(&metadata->subs);
}
SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(deinit_node_metadata_subs);

static struct SelvaModify_HierarchySubscription *find_sub(SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id) {
    struct SelvaModify_HierarchySubscription filter;

    memcpy(&filter.sub_id, sub_id, sizeof(Selva_SubscriptionId));
    return RB_FIND(hierarchy_subscriptions_tree, &hierarchy->subs_head, &filter);
}

static int setSubscriptionMarker(Selva_NodeId id, void *arg, struct SelvaModify_HierarchyMetaData *metadata) {
    struct SelvaModify_HierarchySubscription *sub;

    sub = (struct SelvaModify_HierarchySubscription *)arg;
    fprintf(stderr, "Set sub %s to %.*s\n",
            subId2str(sub->sub_id), (int)SELVA_NODE_ID_SIZE, id);
    SVector_InsertFast(&metadata->subs, sub);

    return 0;
}

static int clearSubscriptionMarker(Selva_NodeId id, void *arg, struct SelvaModify_HierarchyMetaData *metadata) {
    struct SelvaModify_HierarchySubscription *sub;

    sub = (struct SelvaModify_HierarchySubscription *)arg;
    fprintf(stderr, "Clear sub %s from %.*s (nr_subs: %zd)\n",
            subId2str(sub->sub_id), (int)SELVA_NODE_ID_SIZE, id,
            SVector_Size(&metadata->subs));
    SVector_Remove(&metadata->subs, sub);

    return 0;
}

/**
 * Create or update a subscription.
 */
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

    sub = find_sub(hierarchy, sub_id);

    if (!sub) {
        sub = RedisModule_Alloc(sizeof(struct SelvaModify_HierarchySubscription));
        if (!sub) {
            return SELVA_MODIFY_HIERARCHY_ENOMEM;
        }

        memcpy(sub->sub_id, sub_id, sizeof(sub->sub_id));
        memcpy(sub->sub_node_id, node_id, SELVA_NODE_ID_SIZE);
        sub->sub_type = type;
        sub->sub_filter = NULL;

        /*
         * Add to the list of subscriptions.
         */
        if (unlikely(RB_INSERT(hierarchy_subscriptions_tree, &hierarchy->subs_head, sub) != NULL)) {
            RedisModule_Free(sub);
            return SELVA_MODIFY_HIERARCHY_EEXIST;
        }
    }

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

/**
 * Clear subscription starting from node_id.
 * Clear subscription starting from node_id and remove the subscription if
 * node_id is the starting point.
 */
static void clear_sub(struct SelvaModify_Hierarchy *hierarchy, struct SelvaModify_HierarchySubscription *sub, Selva_NodeId node_id) {
    enum SelvaModify_HierarchyTraversal dir;
    struct SelvaModify_HierarchyCallback cb = {
        .node_cb = clearSubscriptionMarker,
        .node_arg = sub,
    };

    switch (sub->sub_type) {
    case SELVA_SUBSCRIPTION_TYPE_ANCESTORS:
        dir = SELVA_MODIFY_HIERARCHY_DFS_ANCESTORS;
        break;
    case SELVA_SUBSCRIPTION_TYPE_DESCENDANTS:
        dir = SELVA_MODIFY_HIERARCHY_DFS_DESCENDANTS;
        break;
    default:
        fprintf(stderr, "Hierarchy: Subscription deletion failed: Invalid subscription %.*s",
                (int)(2 * sizeof(Selva_SubscriptionId)), subId2str(sub->sub_id));
        return;
    }

    /* Remove subscription markers. */
    (void)SelvaModify_TraverseHierarchy(hierarchy, node_id, dir, &cb);

    /*
     * TODO To delete the subscription when it has been deleted from all nodes
     * we need to check if it still exists in the subscription root node.
     */
}

void SelvaModify_DeleteSubscription(struct SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id) {
    struct SelvaModify_HierarchySubscription *sub;

    sub = find_sub(hierarchy, sub_id);
    if (sub) {
        clear_sub(hierarchy, sub, sub->sub_node_id);
    }
}

void SelvaModify_ClearAllSubscriptionMarkers(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetaData *metadata) {
    const size_t nr_subs = SVector_Size(&metadata->subs);
    svector_autofree SVector subs = {0};
    struct SelvaModify_HierarchySubscription **sub_pp;

    if (nr_subs == 0) {
        return;
    }

    fprintf(stderr, "Removing %zu subscriptions from %.*s\n",
            nr_subs, (int)SELVA_NODE_ID_SIZE, node_id);

    if (!SVector_Clone(&subs, &metadata->subs, NULL)) {
        fprintf(stderr, "Hierarchy: Subs ENOMEM\n");
        return;
    }

    /*
     * Remove each subscription from this node and its ancestors/descendants.
     */
    SVECTOR_FOREACH(sub_pp, &subs) {
        struct SelvaModify_HierarchySubscription *sub = *sub_pp;

        clear_sub(hierarchy, sub, node_id);
    }
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

        byte[0] = arg_str[2 * i];
        byte[1] = arg_str[2 * i + 1];
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
 * KEY
 */
int SelvaModify_Hierarchy_SubscriptionsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 2) {
        return RedisModule_WrongArity(ctx);
    }

    const size_t ARGV_REDIS_KEY = 1;

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    struct SelvaModify_HierarchySubscription *sub;
    size_t array_len = 0;

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    RB_FOREACH(sub, hierarchy_subscriptions_tree, &hierarchy->subs_head) {
        RedisModule_ReplyWithStringBuffer(ctx, subId2str(sub->sub_id), 2 * sizeof(Selva_SubscriptionId));
        array_len++;
    }

    RedisModule_ReplySetArrayLength(ctx, array_len);

    return REDISMODULE_OK;
}

/*
 * KEY SUB_ID
 */
int SelvaModify_Hierarchy_UnsubscribeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    int err;

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    const size_t ARGV_REDIS_KEY = 1;
    const size_t ARGV_SUB_ID    = 2;

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

    struct SelvaModify_HierarchySubscription *sub;
    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        RedisModule_ReplyWithLongLong(ctx, 0);
        return REDISMODULE_OK;
    }

    clear_sub(hierarchy, sub, sub->sub_node_id);

    /*
     * TODO Might want to have a function for deleting a subscription.
     */
    RB_REMOVE(hierarchy_subscriptions_tree, &hierarchy->subs_head, sub);
    RedisModule_Free(sub);

    RedisModule_ReplyWithLongLong(ctx, 1);
    return REDISMODULE_OK;
}

static int Hierarchy_Subscriptions_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.subscribe", SelvaModify_Hierarchy_SubscribeCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.subscriptions", SelvaModify_Hierarchy_SubscriptionsCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.unsubscribe", SelvaModify_Hierarchy_UnsubscribeCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_MODIFY_ONLOAD(Hierarchy_Subscriptions_OnLoad);
