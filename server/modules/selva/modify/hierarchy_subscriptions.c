#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "async_task.h"
#include "cdefs.h"
#include "hierarchy.h"
#include "redismodule.h"
#include "rpn.h"

enum subscription_type {
    SUBSCRIPTION_TYPE_ANCESTORS,
    SUBSCRIPTION_TYPE_DESCENDANTS,
};
typedef unsigned char Selva_SubscriptionId[32];

struct SelvaModify_HierarchySubscription {
    Selva_SubscriptionId sub_id;
    Selva_NodeId nodeId;
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

struct SelvaModify_HierarchySubscription *SelvaModify_CreateSubscription(Selva_SubscriptionId sub_id) {
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

    return sub;
}

void SelvaModify_DestroySubscription(Selva_SubscriptionId sub_id) {
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

        /* TODO Remove pointers to the subscription. */

        RedisModule_Free(sub);
    }
}

static int sendAncestorEvent(Selva_NodeId id, __unused void *arg) {
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
