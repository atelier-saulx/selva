#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "async_task.h"
#include "cdefs.h"
#include "hierarchy.h"
#include "redismodule.h"
#include "rpn.h"
#include "svector.h"

enum subscription_type {
    SUBSCRIPTION_TYPE_ANCESTORS,
    SUBSCRIPTION_TYPE_DESCENDANTS,
};
typedef unsigned char Selva_SubscriptionId[32];

struct SelvaModify_HierarchySubscription {
    Selva_SubscriptionId sub_id;
    Selva_NodeId sub_nodeId;
    uint16_t _spare;
    enum subscription_type sub_type;
    rpn_token *sub_filter;
    struct SelvaModify_HierarchySubscription *next_sub;
};

static struct SelvaModify_HierarchySubscription *subs_head;

static const char ancestors_field_str[] = "ancestors";
#define ANCESTORS_FIELD_LEN (sizeof(ancestors_field_str) - 1)
#define ANCESTORS_EVENT_PAYLOAD_LEN \
    (sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + ANCESTORS_FIELD_LEN)

static int subscription_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const struct SelvaModify_HierarchySubscription *a = *(const struct SelvaModify_HierarchySubscription **)a_raw;
    const struct SelvaModify_HierarchySubscription *b = *(const struct SelvaModify_HierarchySubscription **)b_raw;

    return memcmp(a->sub_id, b->sub_id, sizeof(Selva_SubscriptionId));
}

static void init_subs(Selva_NodeId id, struct SelvaModify_HierarchyMetaData *metadata) {
    SVector_Init(&metadata->subs, 1, subscription_compare);
}
SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(init_subs);

static void deinit_subs(Selva_NodeId id, struct SelvaModify_HierarchyMetaData *metadata) {
    /* TODO Delete subscriptions? */
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

struct SelvaModify_HierarchySubscription *SelvaModify_CreateSubscription(
		struct SelvaModify_Hierarchy *hierarchy,
		Selva_SubscriptionId sub_id) {
    struct SelvaModify_HierarchySubscription *sub;

    sub = RedisModule_Alloc(sizeof(struct SelvaModify_HierarchySubscription));
    if (!sub) {
        return NULL;
    }

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
    (void)SelvaModify_TraverseHierarchy(hierarchy, sub->sub_nodeId, SELVA_MODIFY_HIERARCHY_DFS_DESCENDANTS, &cb);

    return sub;
}

void SelvaModify_DestroySubscription(struct SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id) {
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
        /* Remove from the list. */
        prev_sub->next_sub = sub->next_sub;

        /*
         * Remove subscription markers.
         */
        struct SelvaModify_HierarchyCallback cb = {
            .node_cb = clearSubscriptionMarker,
            .node_arg = sub,
        };
        (void)SelvaModify_TraverseHierarchy(hierarchy, sub->sub_nodeId, SELVA_MODIFY_HIERARCHY_DFS_DESCENDANTS, &cb);

        RedisModule_Free(sub);
    }
}

static int sendAncestorEvent(Selva_NodeId id, __unused void *arg, __unused struct SelvaModify_HierarchyMetaData *metadata) {
    SelvaModify_PublishUpdate(id, ancestors_field_str, ANCESTORS_FIELD_LEN);

    return 0;
}

void SelvaModify_PublishDescendants(struct SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
    struct SelvaModify_HierarchyCallback cb = {
        .node_cb = sendAncestorEvent,
        .node_arg = NULL,
    };

    (void)SelvaModify_TraverseHierarchy(hierarchy, id, SELVA_MODIFY_HIERARCHY_DFS_DESCENDANTS, &cb);
}
