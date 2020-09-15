#pragma once
#ifndef SELVA_MODIFY_SUBSCRIPTIONS
#define SELVA_MODIFY_SUBSCRIPTIONS

#include "selva.h"
#include "svector.h"

#define SELVA_SUBSCRIPTION_FLAG_TRAVERSING      0x0001

/**
 * Hierarchy changed.
 */
#define SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY    0x0002

/**
 * Field changed.
 */
#define SELVA_SUBSCRIPTION_FLAG_CH_FIELD        0x0004

struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyMetadata;
struct hierarchy_subscriptions_tree;

/**
 * A data structure for subscription markers.
 */
struct Selva_SubscriptionMarkers {
    /**
     * A list of subscriptionMarker structures.
     */
    struct SVector vec;
    /* *
     * Lookup filter.
     * All flags from sub_markers OR'ed for faster lookup.
     */
    unsigned flags_filter;
};

/**
 * A structure for deferring subscription events.
 */
struct SelvaSubscriptions_DeferredEvents {
    SVector subs;
};

/**
 * Selva subscription ID to hex string.
 */
char *Selva_SubscriptionId2str(char dest[SELVA_SUBSCRIPTION_ID_STR_LEN + 1], const Selva_SubscriptionId sub_id);

int Selva_SubscriptionStr2id(Selva_SubscriptionId dest, const char *src);

int SelvaSubscriptions_InitMarkersStruct(struct Selva_SubscriptionMarkers *markers);
void SelvaSubscriptions_DestroyAll(struct SelvaModify_Hierarchy *hierarchy);
int SelvaSubscriptions_Refresh(struct SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id);
void SelvaSubscriptions_RefreshByMarker(struct SelvaModify_Hierarchy *hierarchy, struct SVector *markers);
void SelvaSubscriptions_Delete(struct SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id);
void SelvaSubscriptions_ClearAllMarkers(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetadata *metadata);


int SelvaSubscriptions_InitDeferredEvents(struct SelvaModify_Hierarchy *hierarchy);
void SelvaSubscriptions_DestroyDeferredEvents(struct SelvaModify_Hierarchy *hierarchy);
void SelvaSubscriptions_DeferHierarchyEvents(struct SelvaModify_Hierarchy *hierarchy,
                                             const struct SelvaModify_HierarchyMetadata *metadata);
void SelvaSubscriptions_DeferFieldChangeEvents(struct SelvaModify_Hierarchy *hierarchy,
                                               const Selva_NodeId node_id,
                                               const struct SelvaModify_HierarchyMetadata *metadata,
                                               const char *field);
void SelvaSubscriptions_SendDeferredEvents(struct SelvaModify_Hierarchy *hierarchy);

#endif /* SELVA_MODIFY_SUBSCRIPTIONS */
