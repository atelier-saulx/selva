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
 * Alias changed.
 * Alias moved or deleted.
 * This flag also acts a as modifier and it clears the markers of the
 * subscription after an event is deferred.
 */
#define SELVA_SUBSCRIPTION_FLAG_ALIAS           0x0008

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

/**
 * Trigger marker.
 * Send a subscription trigger event.
 */
#define SELVA_SUBSCRIPTION_FLAG_TRIGGER         0x0410

#define SELVA_SUBSCRIPTION_MATCHER_FLAGS_MASK   0x00ff
#define SELVA_SUBSCRIPTION_MODIFIER_FLAGS       0xff00

/*
 * End of Subscription Marker Flags.
 */

struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyMetadata;
struct hierarchy_subscriptions_tree;
struct Selva_Subscription;

enum Selva_SubscriptionTriggerType {
    SELVA_SUBSCRIPTION_TRIGGER_TYPE_CREATED = 0,
    SELVA_SUBSCRIPTION_TRIGGER_TYPE_UPDATED,
    SELVA_SUBSCRIPTION_TRIGGER_TYPE_DELETED,
};

/**
 * Subscription marker.
 */
struct Selva_SubscriptionMarker {
    Selva_SubscriptionMarkerId marker_id;
    unsigned short marker_flags;
    enum SelvaModify_HierarchyTraversal dir;
    union {
        /*
         * node_id is never used when SELVA_SUBSCRIPTION_FLAG_TRIGGER is set.
         */
        Selva_NodeId node_id;
        enum Selva_SubscriptionTriggerType event_type;
    };
    const char *ref_field; /*!< Ref field to traverse. */
    struct rpn_ctx *filter_ctx;
    rpn_token *filter_expression;
    char *fields; /* \n separated and \0 terminated. */

    /*
     * Temp storage for tracking filter result changes.
     * Temp storage for tracking changes during a selva.modify command.
     * Anything using these should assume that the initial value is undefined
     * before SelvaSubscriptions_FieldChangePrecheck() is called.
     */
    struct {
        /**
         * The node that is undergoing a change.
         * This shall be used as an invariant between function calls to
         * subscriptions.
         */
        Selva_NodeId node_id;
        /**
         * Filter result on SelvaSubscriptions_FieldChangePrecheck().
         */
        int res;
    } filter_history;
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
    unsigned short flags_filter;
};

/**
 * A structure for deferring subscription events.
 */
struct SelvaSubscriptions_DeferredEvents {
    SVector updates; /*!< A set of Selva_Subscriptions. */
    SVector triggers; /*!< A set of Selva_SubscriptionMarkers */
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
int Selva_AddSubscriptionAliasMarker(
        struct RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id,
        struct RedisModuleString *alias_name,
        Selva_NodeId node_id
    );
struct Selva_SubscriptionMarker *SelvaSubscriptions_GetMarker(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id);
void Selva_Subscriptions_SetMarker(
        const Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetadata *metadata,
        struct Selva_SubscriptionMarker *marker);
void SelvaSubscriptions_ClearAllMarkers(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetadata *metadata);

void SelvaSubscriptions_DeferMissingAccessorEvents(struct SelvaModify_Hierarchy *hierarchy, const char *id_str, size_t id_len);
int SelvaSubscriptions_InitDeferredEvents(struct SelvaModify_Hierarchy *hierarchy);
void SelvaSubscriptions_DestroyDeferredEvents(struct SelvaModify_Hierarchy *hierarchy);
void SelvaSubscriptions_InheritParent(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetadata *node_metadata,
        size_t node_nr_children,
        const Selva_NodeId parent_id,
        struct SelvaModify_HierarchyMetadata *parent_metadata);
void SelvaSubscriptions_InheritChild(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetadata *node_metadata,
        size_t node_nr_parents,
        const Selva_NodeId child_id,
        struct SelvaModify_HierarchyMetadata *child_metadata);
void SelvaSubscriptions_DeferHierarchyEvents(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata);
void SelvaSubscriptions_DeferHierarchyDeletionEvents(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata);
void SelvaSubscriptions_FieldChangePrecheck(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata);
void SelvaSubscriptions_DeferFieldChangeEvents(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata,
        const char *field);
void Selva_Subscriptions_DeferAliasChangeEvents(
        struct RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        struct RedisModuleString *alias_name);
void Selva_Subscriptions_DeferTriggerEvents(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        enum Selva_SubscriptionTriggerType event_type);
void SelvaSubscriptions_SendDeferredEvents(struct SelvaModify_Hierarchy *hierarchy);

#endif /* SELVA_MODIFY_SUBSCRIPTIONS */
