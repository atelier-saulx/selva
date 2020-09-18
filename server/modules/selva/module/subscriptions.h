#pragma once
#ifndef SELVA_MODIFY_SUBSCRIPTIONS
#define SELVA_MODIFY_SUBSCRIPTIONS

#include "selva.h"
#include "svector.h"
#include "rpn.h"

/*
 * Subscription Marker Flags
 * -------------------------
 *
 *  Flags from 0x0001 to 0x0080 are reserved for matcher flags that are used to
 *  check whether an event should be sent on particular kind of change in the
 *  database. These flags are essentially type information.
 *
 *  Flags from 0x0100 to 0x8000 are reserved for modifier flags modifying the
 *  match result or the behavior of the marker. These flags may for example
 *  make an early short-circuit to skip the matching logic even when the
 *  matcher flags would cause a check. These flags are never included in the
 *  flags_filters.
 */

/**
 * Hierarchy changed.
 * Send an event if children or parents changes.
 */
#define SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY    0x0002

/**
 * Field changed.
 * Send an event if a named field changes.
 */
#define SELVA_SUBSCRIPTION_FLAG_CH_FIELD        0x0004

/**
 * Reference subscription.
 * Ignores changes to the root node of the marker and only
 * sends events for changes to referenced nodes. I.e. when
 * node_id != marker->node_id.
 */
#define SELVA_SUBSCRIPTION_FLAG_REF             0x0100

/**
 * Detached marker.
 * The marker should not be applied to nodes directly regardless
 * whether tarversal direction is set.
 */
#define SELVA_SUBSCRIPTION_FLAG_DETACH          0x0200

#define SELVA_SUBSCRIPTION_MATCHER_FLAGS_MASK   0x00ff
#define SELVA_SUBSCRIPTION_MODIFIER_FLAGS       0xff00

/*
 * End of Subscription Marker Flags.
 */

struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyMetadata;
struct hierarchy_subscriptions_tree;
struct Selva_Subscription;

/**
 * Subscription marker.
 */
struct Selva_SubscriptionMarker {
    Selva_SubscriptionMarkerId marker_id;
    unsigned marker_flags;
    Selva_NodeId node_id;
    enum SelvaModify_HierarchyTraversal dir;
    struct rpn_ctx *filter_ctx;
    rpn_token *filter_expression;
    char *fields; /* \n separated and \0 terminated. */
    struct Selva_Subscription *sub; /* Pointer back to the subscription. */
};

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
struct Selva_SubscriptionMarker *SelvaSubscriptions_GetMarker(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id);
void Selva_Subscriptions_SetMarker(
        struct SelvaModify_HierarchyMetadata *metadata,
        struct Selva_SubscriptionMarker *marker);
void SelvaSubscriptions_ClearAllMarkers(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetadata *metadata);


int SelvaSubscriptions_InitDeferredEvents(struct SelvaModify_Hierarchy *hierarchy);
void SelvaSubscriptions_DestroyDeferredEvents(struct SelvaModify_Hierarchy *hierarchy);
void SelvaSubscriptions_DeferHierarchyEvents(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata);
void SelvaSubscriptions_DeferFieldChangeEvents(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata,
        const char *field);
void SelvaSubscriptions_SendDeferredEvents(struct SelvaModify_Hierarchy *hierarchy);

#endif /* SELVA_MODIFY_SUBSCRIPTIONS */
