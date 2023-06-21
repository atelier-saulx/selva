/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef SELVA_MODIFY_SUBSCRIPTIONS
#define SELVA_MODIFY_SUBSCRIPTIONS

#include "selva_db.h"
#include "traversal.h"
#include "util/svector.h"
#include "rpn.h"

struct SelvaHierarchy;
struct SelvaHierarchyCallback;
struct SelvaHierarchyMetadata;
struct Selva_Subscription;
struct Selva_SubscriptionMarker;
struct hierarchy_subscriptions_tree;
struct selva_string;

/**
 * Subscription Marker Flags.
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
 *  TODO Think about the naming of the flags.
 *  TODO A dirty flag would be useful to tell whether a refresh is still needed and
 *  if not the refresh could be skipped when attempted.
 */
enum SelvaSubscriptionsMarkerFlags {
    /**
     * Marker cleared on a hierarchy change.
     * The marker must be refreshed in order to keep receiving updates.
     */
    SELVA_SUBSCRIPTION_FLAG_CL_HIERARCHY = 0x0001,

    /**
     * Hierarchy changed event.
     */
    SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY = 0x0002,

    /**
     * Field changed.
     * Matches if a named field changes.
     */
    SELVA_SUBSCRIPTION_FLAG_CH_FIELD = 0x0004,

    /**
     * Alias changed.
     * Matches if an alias is moved or deleted.
     * This flag also acts a as modifier and it clears the markers of the
     * subscription after an event is deferred.
     * The action function is called only if this flag is also set on the marker.
     */
    SELVA_SUBSCRIPTION_FLAG_CH_ALIAS = 0x0008,

    /**
     * Refresh marked this node.
     * If this flag is set on the marker then the action function is called
     * for each node that is marked on refresh.
     */
    SELVA_SUBSCRIPTION_FLAG_REFRESH = 0x0020,

    /**
     * Reference subscription.
     * Ignores changes to the root node of the marker and only
     * sends events for changes to referenced nodes. I.e. when
     * node_id != marker->node_id.
     */
    SELVA_SUBSCRIPTION_FLAG_REF = 0x0100,

    /**
     * Detached marker.
     * The marker should not be applied to nodes directly regardless
     * whether tarversal direction is set.
     */
    SELVA_SUBSCRIPTION_FLAG_DETACH = 0x0200,

    /**
     * Trigger marker.
     * Send a subscription trigger event.
     */
    SELVA_SUBSCRIPTION_FLAG_TRIGGER = 0x0410,

    SELVA_SUBSCRIPTION_MATCHER_FLAGS_MASK = 0x00ff,
    SELVA_SUBSCRIPTION_MODIFIER_FLAGS = 0xff00,
};

/**
 * Trigger event types.
 */
enum Selva_SubscriptionTriggerType {
    /**
     * Node created.
     */
    SELVA_SUBSCRIPTION_TRIGGER_TYPE_CREATED = 0,
    /**
     * Node updated.
     */
    SELVA_SUBSCRIPTION_TRIGGER_TYPE_UPDATED,
    /**
     * Node deleted.
     */
    SELVA_SUBSCRIPTION_TRIGGER_TYPE_DELETED,
};

/**
 * A callback that makes the actual defer action when a marker matches.
 * The action function must not hold a pointer to the node after the function
 * returns as the pointer is not guaranteed to be valid after the call.
 * @param field_str is set if SELVA_SUBSCRIPTION_FLAG_CH_FIELD is set in event_flags.
 */
typedef void Selva_SubscriptionMarkerAction(
        struct SelvaHierarchy *hierarchy,
        struct Selva_SubscriptionMarker *marker,
        unsigned short event_flags,
        const char *field_str,
        size_t field_len,
        struct SelvaHierarchyNode *node);

/**
 * Subscription marker.
 */
struct Selva_SubscriptionMarker {
    Selva_SubscriptionMarkerId marker_id;
    unsigned short marker_flags;

    enum SelvaTraversal dir;
    union {
        /*
         * node_id is never used when SELVA_SUBSCRIPTION_FLAG_TRIGGER is set.
         */
        Selva_NodeId node_id;
        enum Selva_SubscriptionTriggerType event_type;
    };
    /*
     * Which one is used depends on dir.
     */
    union {
        char *ref_field; /*!< Ref field name for traversal when dir requires it. */
        struct rpn_expression *traversal_expression; /*!< Used when SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION. */
    };
    struct rpn_ctx *filter_ctx;
    struct rpn_expression *filter_expression;

    /**
     * A function that should defer an action that will be typically taken at later moment.
     * Typically this function should add the subscription in one of the
     * deferred events lists and eventually SelvaSubscriptions_SendDeferredEvents
     * will be called to handle the event. Normally all this mangling is done just
     * to dedup multiple events for the same subscription but this function
     * pointer can be alternatively used to execute or defer any action.
     */
    Selva_SubscriptionMarkerAction *marker_action;
    /**
     * A pointer to optional data for the action to grab the required context.
     */
    void *marker_action_owner_ctx;

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
    char fields[]; /* \n separated and \0 terminated list of field names considered for change events. */
};

/**
 * A data structure for subscription markers.
 */
struct Selva_SubscriptionMarkers {
    /**
     * Lookup filter.
     * All flags from sub_markers OR'ed for faster lookup.
     */
    unsigned short flags_filter;
    /**
     * A list of pointers to subscriptionMarker structures.
     */
    struct SVector vec;
};

/**
 * A structure for deferring subscription events.
 */
struct SelvaSubscriptions_DeferredEvents {
    SVector updates; /*!< A set of Selva_Subscriptions. */
    SVector triggers; /*!< A set of Selva_SubscriptionMarkers */
};

/**
 * A structure used for sending subscription messages over selva_pubsub.
 */
struct SelvaSubscriptions_PubsubMessage {
    enum {
        SELVA_SUB_UPDATE = 1,
        SELVA_SUB_TRIGGER = 2,
    } __packed event_type;
    Selva_SubscriptionId sub_id;
    Selva_NodeId node_id;
};

#define SELVA_SUBSCRIPTIONS_PUBSUB_CH_ID 0

int SelvaSubscriptions_hasActiveMarkers(const struct SelvaHierarchyMetadata *node_metadata);

/**
 * Generate a subscription marker id by hashing an input string.
 * Sometimes we need to come up with an id for a subscription marker without the
 * clients help and this function is here for those occasions.
 * The generated id will have the MSB set, making it impossible for the client
 * to generate such an id. This makes it easier to recognize server generated
 * ids as well as avoids collision with the client generated ids.
 * @param prev is the previous hash result in case the caller wants to hash multiple input values; Otherwise 0.
 * @param str is a nul-terminated input string that will be hashed.
 * @returns The function returns a new subscription marker id.
 */
Selva_SubscriptionMarkerId Selva_GenSubscriptionMarkerId(Selva_SubscriptionMarkerId prev, const char *s);

/**
 * Init subscriptions data structures in the hierarchy.
 */
void SelvaSubscriptions_InitHierarchy(struct SelvaHierarchy *hierarchy);

/**
 * Destroy all data structures of the subscriptions subsystem and cancel all deferred events.
 */
void SelvaSubscriptions_DestroyAll(struct SelvaHierarchy *hierarchy);

/**
 * Refresh a marker by id.
 * Note that in contrary to other refresh functions this one will only traverse
 * and refresh a single marker of the given subscription.
 */
int SelvaSubscriptions_RefreshByMarkerId(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id);

/**
 * Refresh all subscription markers of a given subscription id.
 */
int SelvaSubscriptions_Refresh(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id);

/**
 * Refresh all subscriptions found in markers SVector.
 */
void SelvaSubscriptions_RefreshByMarker(
        struct SelvaHierarchy *hierarchy,
        const struct SVector *markers);

/**
 * Delete a subsscription and all of its markers by subscription id.
 */
void SelvaSubscriptions_Delete(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id);

/**
 * Delete a single marker from a subscription.
 */
int SelvaSubscriptions_DeleteMarker(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id);

/**
 * Delete a single marker from a subscription by a pointer to the marker.
 */
int SelvaSubscriptions_DeleteMarkerByPtr(
        struct SelvaHierarchy *hierarchy,
        struct Selva_SubscriptionMarker *marker);

/**
 * Add a marker to a hierarchy alias.
 * @param alias_name is a pointer to the alias name.
 * @param node_id is the node the alias is currently pointing at.
 */
int Selva_AddSubscriptionAliasMarker(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id,
        struct selva_string *alias_name,
        Selva_NodeId node_id);

/**
 * Add a subscription marker with a callback.
 * @param filter is an RPN expression used to determine if the callback should be called for this node.
 * @param callback is called each time all the marker conditions are met for a node.
 */
int SelvaSubscriptions_AddCallbackMarker(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id,
        unsigned short marker_flags,
        const Selva_NodeId node_id,
        enum SelvaTraversal dir,
        const char *dir_field,
        const char *dir_expression_str,
        const char *filter_str,
        Selva_SubscriptionMarkerAction *callback,
        void *owner_ctx
    );

/**
 * Get a pointer to a subscription marker by subscription and marker id.
 */
struct Selva_SubscriptionMarker *SelvaSubscriptions_GetMarker(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id);

/**
 * Clear all markers from a node and any nodes along the dir path.
 * Calling this will also defer an event for each subscription that had its
 * marker removed from a node.
 */
void SelvaSubscriptions_ClearAllMarkers(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node);

/**
 * Test if the RPN filter defined in the marker matches.
 * This function is particularly useful for the callback function of a callback
 * marker (added with SelvaSubscriptions_AddCallbackMarker()) as in that case
 * the filter is not executed by the subscriptions system.
 * @param node is a pointer to the node the filter should be executed against.
 * @param marker is a pointer to the subscription marker.
 */
int Selva_SubscriptionFilterMatch(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct Selva_SubscriptionMarker *marker);

/**
 * Destroy all deferred events.
 */
void SelvaSubscriptions_DestroyDeferredEvents(struct SelvaHierarchy *hierarchy);

/**
 * Inherit subscription markers from a parent to child nodes.
 */
void SelvaSubscriptions_InheritParent(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        struct SelvaHierarchyMetadata *node_metadata,
        size_t node_nr_children,
        struct SelvaHierarchyNode *parent);

/**
 * Inherit subscription markers from a child to parent nodes.
 * This function makes sense if there are markers traversing upwards.
 */
void SelvaSubscriptions_InheritChild(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        struct SelvaHierarchyMetadata *node_metadata,
        size_t node_nr_parents,
        struct SelvaHierarchyNode *child);

/**
 * Inherit subscription markers over an edge field.
 * @param field_str is a pointer to the name of the source field.
 */
void SelvaSubscriptions_InheritEdge(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *src_node,
        struct SelvaHierarchyNode *dst_node,
        const char *field_str,
        size_t field_len);

/**
 * Defer an event if id_str was a missing accessor a subscription was waiting for.
 */
void SelvaSubscriptions_DeferMissingAccessorEvents(struct SelvaHierarchy *hierarchy, const char *id_str, size_t id_len);

void SelvaSubscriptions_DeferHierarchyEvents(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node);
void SelvaSubscriptions_DeferHierarchyDeletionEvents(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node);
void SelvaSubscriptions_FieldChangePrecheck(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node);
void SelvaSubscriptions_DeferFieldChangeEvents(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len);
void SelvaSubscriptions_DeferAliasChangeEvents(
        struct SelvaHierarchy *hierarchy,
        struct selva_string *alias_name);
void SelvaSubscriptions_DeferTriggerEvents(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        enum Selva_SubscriptionTriggerType event_type);
void SelvaSubscriptions_SendDeferredEvents(struct SelvaHierarchy *hierarchy);
void SelvaSubscriptions_ReplyWithMarker(struct selva_server_response_out *resp, struct Selva_SubscriptionMarker *marker);

#endif /* SELVA_MODIFY_SUBSCRIPTIONS */
