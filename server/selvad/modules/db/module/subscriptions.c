/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "jemalloc.h"
#include "util/cstrings.h"
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "util/svector.h"
#include "selva_db.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "arg_parser.h"
#include "hierarchy.h"
#include "resolve.h"
#include "rpn.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_trace.h"
#include "subscriptions.h"

struct Selva_Subscription {
    Selva_SubscriptionId sub_id;
    RB_ENTRY(Selva_Subscription) _sub_index_entry;
    SVector markers; /* struct Selva_SubscriptionMarker */
};

struct set_node_marker_data {
    struct Selva_SubscriptionMarker *marker;
};

static const struct SelvaArgParser_EnumType trigger_event_types[] = {
    {
        .name = "created",
        .id = SELVA_SUBSCRIPTION_TRIGGER_TYPE_CREATED,
    },
    {
        .name = "updated",
        .id = SELVA_SUBSCRIPTION_TRIGGER_TYPE_UPDATED,
    },
    {
        .name = "deleted",
        .id = SELVA_SUBSCRIPTION_TRIGGER_TYPE_DELETED,
    },
    {
        .name = NULL,
        .id = 0,
    }
};

static void Selva_Subscription_reply(struct selva_server_response_out *resp, void *p);
static const struct SelvaObjectPointerOpts subs_missing_obj_opts = {
    .ptr_type_id = SELVA_OBJECT_POINTER_SUBS_MISSING,
    .ptr_reply = Selva_Subscription_reply,
};

SELVA_TRACE_HANDLE(cmd_subscriptions_refresh);

static struct Selva_Subscription *find_sub(SelvaHierarchy *hierarchy, const Selva_SubscriptionId sub_id);
static void clear_node_sub(struct SelvaHierarchy *hierarchy, struct Selva_SubscriptionMarker *marker, const Selva_NodeId node_id);

static int marker_svector_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    struct marker_diff {
        Selva_SubscriptionId sub_id;
        Selva_SubscriptionMarkerId marker_id;
    };

    const struct Selva_SubscriptionMarker *a = *(const struct Selva_SubscriptionMarker **)a_raw;
    const struct Selva_SubscriptionMarker *b = *(const struct Selva_SubscriptionMarker **)b_raw;

    if (a == b) {
        return 0;
    }

    struct marker_diff da;
    struct marker_diff db;

#if 0
    __builtin_clear_padding(&da);
#endif
    memset(&da, 0, sizeof(struct marker_diff));
    memcpy(da.sub_id, a->sub->sub_id, sizeof(a->sub->sub_id));
    da.marker_id = a->marker_id;

#if 0
    __builtin_clear_padding(&db);
#endif
    memset(&db, 0, sizeof(struct marker_diff));
    memcpy(db.sub_id, b->sub->sub_id, sizeof(b->sub->sub_id));
    db.marker_id = b->marker_id;

    return memcmp(&da, &db, sizeof(struct marker_diff));
}

static int SelvaSubscription_svector_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const struct Selva_Subscription *a = *(const struct Selva_Subscription **)a_raw;
    const struct Selva_Subscription *b = *(const struct Selva_Subscription **)b_raw;

    return memcmp(a->sub_id, b->sub_id, sizeof(Selva_SubscriptionId));
}

static int subscription_rb_compare(const struct Selva_Subscription *a, const struct Selva_Subscription *b) {
    return memcmp(a->sub_id, b->sub_id, sizeof(Selva_SubscriptionId));
}

RB_PROTOTYPE_STATIC(hierarchy_subscriptions_tree, Selva_Subscription, _sub_index_entry, subscription_rb_compare)
RB_GENERATE_STATIC(hierarchy_subscriptions_tree, Selva_Subscription, _sub_index_entry, subscription_rb_compare)

static void defer_update_event(
        struct SelvaHierarchy *hierarchy,
        struct Selva_SubscriptionMarker *marker,
        unsigned short event_flags,
        const char *field_name,
        size_t field_len,
        struct SelvaHierarchyNode *node);
static void defer_trigger_event(
        struct SelvaHierarchy *hierarchy,
        struct Selva_SubscriptionMarker *marker,
        unsigned short event_flags,
        const char *field_name,
        size_t field_len,
        struct SelvaHierarchyNode *node);
static void defer_event_for_traversing_markers(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node);

/**
 * The given marker flags matches to a hierarchy marker of any kind.
 */
static int isHierarchyMarker(unsigned short flags) {
    return !!(flags & SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY);
}

static int isAliasMarker(unsigned short flags) {
    return !!(flags & SELVA_SUBSCRIPTION_FLAG_CH_ALIAS);
}

static int isTriggerMarker(unsigned short flags) {
    return !!(flags & SELVA_SUBSCRIPTION_FLAG_TRIGGER);
}

/**
 * Inhibit a marker event.
 * Return true if no event should be sent for node_id by this marker.
 */
static int inhibitMarkerEvent(const Selva_NodeId node_id, const struct Selva_SubscriptionMarker *marker) {
    /*
     * SELVA_SUBSCRIPTION_FLAG_REF inhibits an event when node_id matches to the
     * root node_id of the marker.
     */
    if ((marker->marker_flags & (SELVA_SUBSCRIPTION_FLAG_REF | SELVA_SUBSCRIPTION_FLAG_TRIGGER)) == SELVA_SUBSCRIPTION_FLAG_REF &&
        !memcmp(node_id, marker->node_id, SELVA_NODE_ID_SIZE)) {
        return 1;
    }

    return 0;
}

/**
 * Check if the field matches to one of fields in list.
 * @param field_str is a nul-terminated field name.
 */
static int field_match(const char *list, const char *field_str, size_t field_len) {
    int match = 0;

    if (list[0] == '\0') {
        /* Empty string equals to a wildcard */
        match = 1;
    } else {
        /* Test if field matches to any of the fields in list. */
        const char *sep = ".";
        char *p;

        match = stringlist_searchn(list, field_str, field_len);

        /* Test for each subfield if there was no exact match. */
        if (!match && (p = strstr(field_str, sep))) {
            do {
                const size_t len = (ptrdiff_t)p++ - (ptrdiff_t)field_str;

                match = stringlist_searchn(list, field_str, len);
            } while (!match && p && (p = strstr(p, sep)));
        }
    }

    return match;
}

static int contains_hierarchy_fields(const char *list) {
    return list[0] == '\0' /* wildcard */ ||
           field_match(list, SELVA_ANCESTORS_FIELD, sizeof(SELVA_ANCESTORS_FIELD) - 1) ||
           field_match(list, SELVA_CHILDREN_FIELD, sizeof(SELVA_CHILDREN_FIELD) - 1) ||
           field_match(list, SELVA_DESCENDANTS_FIELD, sizeof(SELVA_DESCENDANTS_FIELD) - 1) ||
           field_match(list, SELVA_PARENTS_FIELD, sizeof(SELVA_PARENTS_FIELD) - 1);
}

/**
 * Check if field matches to any of the fields specified in the marker.
 */
static int Selva_SubscriptionFieldMatch(const struct Selva_SubscriptionMarker *marker, const char *field_str, size_t field_len) {
    int match = 0;

    if (!!(marker->marker_flags & SELVA_SUBSCRIPTION_FLAG_CH_FIELD)) {
        match = field_match(marker->fields, field_str, field_len);
    }

    return match;
}

int Selva_SubscriptionFilterMatch(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct Selva_SubscriptionMarker *marker) {
    struct rpn_ctx *filter_ctx = marker->filter_ctx;
    int res = 1; /* When no filter is set the result should be true. */

    if (filter_ctx) {
        Selva_NodeId node_id;
        int err;

        SelvaHierarchy_GetNodeId(node_id, node);
        rpn_set_reg(filter_ctx, 0, node_id, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
        rpn_set_hierarchy_node(filter_ctx, hierarchy, node);
        rpn_set_obj(filter_ctx, SelvaHierarchy_GetNodeObject(node));
        err = rpn_bool(filter_ctx, marker->filter_expression, &res);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Expression failed (node: \"%.*s\"): \"%s\"",
                      (int)SELVA_NODE_ID_SIZE, node_id,
                      rpn_str_error[err]);
            res = 0;
        }
    }

    return res;
}

int SelvaSubscriptions_hasActiveMarkers(const struct SelvaHierarchyMetadata *node_metadata) {
    return SVector_Size(&node_metadata->sub_markers.vec) > 0;
}

Selva_SubscriptionMarkerId Selva_GenSubscriptionMarkerId(Selva_SubscriptionMarkerId prev, const char *s) {
    const Selva_SubscriptionMarkerId auto_flag = (Selva_SubscriptionMarkerId)1 << ((sizeof(Selva_SubscriptionMarkerId) * 8) - 1);
    uint32_t hash;

    /* fnv32 */
    hash = prev > 0 ? (uint32_t)(prev & 0x7FFFFFFF) : 2166136261u;
    for (; *s; s++) {
        hash = (hash ^ *s) * 0x01000193;
    }

    return (Selva_SubscriptionMarkerId)(auto_flag | hash);
}

/*
 * Destroy and free a marker.
 */
__attribute__((nonnull (1))) static void destroy_marker(struct Selva_SubscriptionMarker *marker) {
    SELVA_LOG(SELVA_LOGL_DBG, "Destroying marker %p %" PRImrkId " %.*s",
              marker, marker->marker_id,
              (int)SELVA_NODE_ID_SIZE, marker->node_id);

    rpn_destroy(marker->filter_ctx);
#if MEM_DEBUG
    memset(marker, 0, sizeof(*marker));
#endif
    if (marker->dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                       SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) {
        rpn_destroy_expression(marker->traversal_expression);
    } else {
        selva_free(marker->ref_field);
    }
    rpn_destroy_expression(marker->filter_expression);
    selva_free(marker);
}

static void remove_sub_missing_accessor_markers(SelvaHierarchy *hierarchy, const struct Selva_Subscription *sub) {
    struct SelvaObject *missing = GET_STATIC_SELVA_OBJECT(&hierarchy->subs.missing);
    SelvaObject_Iterator *it_missing;
    struct SelvaObject *subs;
    const char *nodeIdOrAlias;
    char sub_id[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

    if (!sub) {
        return;
    }

    Selva_SubscriptionId2str(sub_id, sub->sub_id);

    it_missing = SelvaObject_ForeachBegin(missing);
    while ((subs = SelvaObject_ForeachValue(missing, &it_missing, &nodeIdOrAlias, SELVA_OBJECT_OBJECT))) {
        /* Delete this subscription stored under nodeIdOrAlias. */
        SelvaObject_DelKeyStr(subs, sub_id, SELVA_SUBSCRIPTION_ID_STR_LEN);

        /* Delete the id key if the object is now empty. */
        if (SelvaObject_Len(subs, NULL) == 0) {
            SelvaObject_DelKeyStr(missing, nodeIdOrAlias, strlen(nodeIdOrAlias));
        }
    }
}

/**
 * Clear and destroy a marker that has been already removed from the subscription.
 * The marker must have been removed from the sub->markers svector before
 * this function is called.
 */
static void do_sub_marker_removal(SelvaHierarchy *hierarchy, struct Selva_SubscriptionMarker *marker) {
    if (marker->dir == SELVA_HIERARCHY_TRAVERSAL_NONE ||
            (marker->marker_flags & (SELVA_SUBSCRIPTION_FLAG_DETACH | SELVA_SUBSCRIPTION_FLAG_TRIGGER))) {
        (void)SVector_Remove(&hierarchy->subs.detached_markers.vec, marker);
    } else {
        /*
         * RFE We could skip some stuff here if hierarchy deletion could give us a signal
         * Other markers are normally pointed by one or more nodes in
         * the hierarchy.
         * If ctx is NULL we assume that the whole hierarchy will be
         * destroyed and thus there is no need to clear each marker.
         * ---
         *  There was if (ctx) here
         */
        clear_node_sub(hierarchy, marker, marker->node_id);
    }
    destroy_marker(marker);
}

static int delete_marker(SelvaHierarchy *hierarchy, struct Selva_Subscription *sub, Selva_SubscriptionMarkerId marker_id) {
    struct Selva_SubscriptionMarker find = {
        .marker_id = marker_id,
        .sub = sub,
    };
    struct Selva_SubscriptionMarker *marker;

    marker = SVector_Remove(&sub->markers, &find);
    if (!marker) {
        return SELVA_SUBSCRIPTIONS_ENOENT;
    }

    do_sub_marker_removal(hierarchy, marker);
    return 0;
}

int SelvaSubscriptions_DeleteMarker(
        SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id) {
    struct Selva_Subscription *sub;

    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        return SELVA_SUBSCRIPTIONS_ENOENT;
    }

    return delete_marker(hierarchy, sub, marker_id);
}

int SelvaSubscriptions_DeleteMarkerByPtr(SelvaHierarchy *hierarchy, struct Selva_SubscriptionMarker *marker) {
    return delete_marker(hierarchy, marker->sub, marker->marker_id);
}


/**
 * Remove and destroy all markers of a subscription.
 */
static void remove_sub_markers(SelvaHierarchy *hierarchy, struct Selva_Subscription *sub) {
    if (SVector_Size(&sub->markers) > 0) {
        struct Selva_SubscriptionMarker *marker;

        while ((marker = SVector_Shift(&sub->markers))) {
            do_sub_marker_removal(hierarchy, marker);
        }
        SVector_ShiftReset(&sub->markers);
    }
}

/*
 * Destroy all markers owned by a subscription and destroy the subscription.
 */
static void destroy_sub(SelvaHierarchy *hierarchy, struct Selva_Subscription *sub) {
    /* Destroy markers. */
    remove_sub_markers(hierarchy, sub);

#if 0
    char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
    SELVA_LOG(SELVA_LOGL_DBG, "Destroying sub_id %s", Selva_SubscriptionId2str(str, sub->sub_id));
#endif

    /* Remove missing accessor markers. */
    remove_sub_missing_accessor_markers(hierarchy, sub);

    RB_REMOVE(hierarchy_subscriptions_tree, &hierarchy->subs.head, sub);
    SVector_Destroy(&sub->markers);
#if MEM_DEBUG
    memset(sub, 0, sizeof(*sub));
#endif
    selva_free(sub);
}

/*
 * Destroy all subscription markers and subscriptions.
 */
static void destroy_all_sub_markers(SelvaHierarchy *hierarchy) {
    struct hierarchy_subscriptions_tree *subs_head = &hierarchy->subs.head;
    struct Selva_Subscription *sub;
    struct Selva_Subscription *next;

    for (sub = RB_MIN(hierarchy_subscriptions_tree, subs_head); sub != NULL; sub = next) {
        next = RB_NEXT(hierarchy_subscriptions_tree, subs_head, sub);
        destroy_sub(hierarchy, sub);
    }
}

static void SelvaSubscriptions_InitMarkersStruct(struct Selva_SubscriptionMarkers *markers) {
    SVector_Init(&markers->vec, 0, marker_svector_compare);
    markers->flags_filter = 0;
}

static void SelvaSubscriptions_InitDeferredEvents(struct SelvaHierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    SVector_Init(&def->updates, 2, SelvaSubscription_svector_compare);
    SVector_Init(&def->triggers, 3, marker_svector_compare);
}

void SelvaSubscriptions_InitHierarchy(SelvaHierarchy *hierarchy) {
    RB_INIT(&hierarchy->subs.head);

    SelvaObject_Init(hierarchy->subs.missing._obj_data);

    SelvaSubscriptions_InitMarkersStruct(&hierarchy->subs.detached_markers);
    SelvaSubscriptions_InitDeferredEvents(hierarchy);
}

void SelvaSubscriptions_DestroyAll(SelvaHierarchy *hierarchy) {
    /*
     * If we destroy the defer vectors first then clearing the subs won't be
     * able to defer any events.
     */
    SelvaSubscriptions_DestroyDeferredEvents(hierarchy);

    destroy_all_sub_markers(hierarchy);
    SelvaObject_Destroy(GET_STATIC_SELVA_OBJECT(&hierarchy->subs.missing));

    /*
     * Do this as the last step because destroy_all_sub_markers() will access
     * the vector.
     */
    SVector_Destroy(&hierarchy->subs.detached_markers.vec);
}

static void init_node_metadata_subs(
        const Selva_NodeId id __unused,
        struct SelvaHierarchyMetadata *metadata) {
    SelvaSubscriptions_InitMarkersStruct(&metadata->sub_markers);
}
SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(init_node_metadata_subs);

static void deinit_node_metadata_subs(
        SelvaHierarchy *hierarchy __unused,
        struct SelvaHierarchyNode *node __unused,
        struct SelvaHierarchyMetadata *metadata) {
    SVector_Destroy(&metadata->sub_markers.vec);
}
SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(deinit_node_metadata_subs);

static struct Selva_Subscription *find_sub(SelvaHierarchy *hierarchy, const Selva_SubscriptionId sub_id) {
    struct Selva_Subscription filter;

    memcpy(&filter.sub_id, sub_id, sizeof(Selva_SubscriptionId));
    return RB_FIND(hierarchy_subscriptions_tree, &hierarchy->subs.head, &filter);
}

static struct Selva_SubscriptionMarker *find_sub_marker(
        struct Selva_Subscription *sub,
        Selva_SubscriptionMarkerId marker_id) {
    return SVector_Search(&sub->markers, &(struct Selva_SubscriptionMarker){
        .marker_id = marker_id,
        .sub = sub,
    });
}

static void set_marker(struct Selva_SubscriptionMarkers *sub_markers, struct Selva_SubscriptionMarker *marker) {
    if (!SVector_InsertFast(&sub_markers->vec, marker)) {
        sub_markers->flags_filter |= marker->marker_flags & SELVA_SUBSCRIPTION_MATCHER_FLAGS_MASK;
    }
}

static void reset_marker_filter(struct Selva_SubscriptionMarkers *sub_markers) {
    struct SVectorIterator it;
    const struct Selva_SubscriptionMarker *marker;

    sub_markers->flags_filter = 0;

    SVector_ForeachBegin(&it, &sub_markers->vec);
    while ((marker = SVector_Foreach(&it))) {
        sub_markers->flags_filter |= marker->marker_flags & SELVA_SUBSCRIPTION_MATCHER_FLAGS_MASK;
    }
}

/**
 * Remove marker from sub_markers and update the marker filter.
 */
static void clear_marker(struct Selva_SubscriptionMarkers *sub_markers, struct Selva_SubscriptionMarker *marker) {
    (void)SVector_Remove(&sub_markers->vec, marker);
    reset_marker_filter(sub_markers);
}

/*
 * Set a marker to a node metadata.
 */
static int set_node_marker_cb(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct set_node_marker_data *data = (struct set_node_marker_data *)arg;
    struct Selva_SubscriptionMarker *marker = data->marker;
    struct SelvaHierarchyMetadata *metadata;

    if (marker->dir == SELVA_HIERARCHY_TRAVERSAL_REF) {
        Selva_NodeId node_id;

        if (memcmp(SelvaHierarchy_GetNodeId(node_id, node), marker->node_id, SELVA_NODE_ID_SIZE)) {
            /*
             * ref markers are not propagated beyond the first node because
             * it's impossible to track them. This means that there is no
             * descendants-like (or recursive) behavior available for ref
             * markers.
             */
            return 0;
        }
    }

#if 0
    char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
    Selva_NodeId node_id;

    SelvaHierarchy_GetNodeId(node_id, node);
    SELVA_LOG(SELVA_LOGL_DBG, "Set sub marker %s:%" PRImrkId " to %.*s",
              Selva_SubscriptionId2str(str, marker->sub->sub_id),
              marker->marker_id,
              (int)SELVA_NODE_ID_SIZE, node_id);
#endif

    metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);
    set_marker(&metadata->sub_markers, marker);

    if (marker->marker_flags & SELVA_SUBSCRIPTION_FLAG_REFRESH) {
        unsigned short flags = SELVA_SUBSCRIPTION_FLAG_REFRESH;
        marker->marker_action(hierarchy, marker, flags, NULL, 0, node);
    }

    return 0;
}

static int clear_node_marker_cb(
        struct SelvaHierarchy *hierarchy __unused,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct SelvaHierarchyMetadata *metadata;
    struct Selva_SubscriptionMarker *marker = (struct Selva_SubscriptionMarker*)arg;

    metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);
#if 0
    char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
    Selva_NodeId id;

    SelvaHierarchy_GetNodeId(id, node);
    SELVA_LOG(SELVA_LOGL_DBG, "Clear sub marker %s:%" PRImrkId " (%p start_node_id: %.*s) from node %.*s (nr_subs: %zd)",
              Selva_SubscriptionId2str(str, marker->sub->sub_id),
              marker->marker_id,
              marker,
              (int)SELVA_NODE_ID_SIZE, marker->node_id,
              (int)SELVA_NODE_ID_SIZE, id,
              SVector_Size(&metadata->sub_markers.vec));
#endif

    clear_marker(&metadata->sub_markers, marker);

    return 0;
}

/**
 * Create a subscription.
 */
static struct Selva_Subscription *create_subscription(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id) {
    struct Selva_Subscription *sub;

    sub = selva_calloc(1, sizeof(struct Selva_Subscription));
    memcpy(sub->sub_id, sub_id, sizeof(sub->sub_id));
    SVector_Init(&sub->markers, 1, marker_svector_compare);

    /*
     * Add to the list of subscriptions.
     */
    if (unlikely(RB_INSERT(hierarchy_subscriptions_tree, &hierarchy->subs.head, sub) != NULL)) {
        SVector_Destroy(&sub->markers);
        selva_free(sub);
        return NULL;
    }

    return sub;
}

/**
 * Create a new marker structure.
 * @param fields_str can be NULL; SELVA_SUBSCRIPTION_FLAG_CH_FIELD is implicit if the arg is given.
 */
static int new_marker(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id,
        const char *fields_str,
        size_t fields_len,
        unsigned short flags,
        Selva_SubscriptionMarkerAction *marker_action,
        struct Selva_SubscriptionMarker **out)
{
    struct Selva_Subscription *sub;
    struct Selva_SubscriptionMarker *marker;

    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        sub = create_subscription(hierarchy, sub_id);
        if (!sub) {
            return SELVA_SUBSCRIPTIONS_EINVAL;
        }
    } else {
        if (find_sub_marker(sub, marker_id)) {
            return SELVA_SUBSCRIPTIONS_EEXIST;
        }
    }

    /*
     * Marker is a hierarchy change marker only if it opts for hierarchy
     * field updates. Otherwise hierarchy events are only sent when the
     * subscription needs a refresh.
     */
    if (fields_str) {
        flags |= SELVA_SUBSCRIPTION_FLAG_CH_FIELD;
        if (contains_hierarchy_fields(fields_str)) {
            flags |= SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY;
        }
    }

    marker = selva_calloc(1, sizeof(struct Selva_SubscriptionMarker) + (fields_str ? fields_len + 1 : 0));
    marker->marker_id = marker_id;
    marker->marker_flags = flags;
    marker->dir = SELVA_HIERARCHY_TRAVERSAL_NONE;
    marker->marker_action = marker_action;
    marker->sub = sub;

    if (fields_str) {
        memcpy(marker->fields, fields_str, fields_len);
        marker->fields[fields_len] = '\0';
    }

    (void)SVector_InsertFast(&sub->markers, marker);
    *out = marker;
    return 0;
}

static void marker_set_node_id(struct Selva_SubscriptionMarker *marker, const Selva_NodeId node_id) {
    marker->marker_flags &= ~SELVA_SUBSCRIPTION_FLAG_TRIGGER;
    memcpy(marker->node_id, node_id, SELVA_NODE_ID_SIZE);
}

static void marker_set_dir(struct Selva_SubscriptionMarker *marker, enum SelvaTraversal dir) {
    marker->dir = dir;
}

static void marker_set_trigger(struct Selva_SubscriptionMarker *marker, enum Selva_SubscriptionTriggerType event_type) {
    marker->marker_flags |= SELVA_SUBSCRIPTION_FLAG_TRIGGER; /* Just in case. */
    marker->event_type = event_type;
}

static void marker_set_filter(struct Selva_SubscriptionMarker *marker, struct rpn_ctx *ctx, struct rpn_expression *expression) {
    marker->filter_ctx = ctx;
    marker->filter_expression = expression;
}

static void marker_set_action_owner_ctx(struct Selva_SubscriptionMarker *marker, void *owner_ctx) {
    marker->marker_action_owner_ctx = owner_ctx;
}

/**
 * Set ref_field for the marker.
 * The traversal direction must have been set to one of the ones requiring a ref
 * before calling this function and the SELVA_SUBSCRIPTION_FLAG_TRIGGER flag
 * must not be set.
 * @param ref_field is the field used for traversal that must be a c-string.
 */
static void marker_set_ref_field(struct Selva_SubscriptionMarker *marker, const char *ref_field) {
    assert((marker->dir & (SELVA_HIERARCHY_TRAVERSAL_REF | SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD | SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD)) &&
           !(marker->marker_flags & SELVA_SUBSCRIPTION_FLAG_TRIGGER));

    marker->ref_field = selva_strdup(ref_field);
}

static void marker_set_traversal_expression(struct Selva_SubscriptionMarker *marker, struct rpn_expression *traversal_expression) {
    assert(marker->dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                          SELVA_HIERARCHY_TRAVERSAL_EXPRESSION));

    marker->traversal_expression = traversal_expression;
}

int Selva_AddSubscriptionAliasMarker(
        SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id,
        struct selva_string *alias_name,
        Selva_NodeId node_id
    ) {
    struct Selva_SubscriptionMarker *old_marker;
    struct rpn_ctx *filter_ctx = NULL;
    struct rpn_expression *filter_expression = NULL;
    int err = 0;

    old_marker = SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id);
    if (old_marker) {
        if (memcmp(old_marker->node_id, node_id, SELVA_NODE_ID_SIZE)) {
            TO_STR(alias_name);

            SELVA_LOG(SELVA_LOGL_WARN,
                      "Alias marker \"%.*s\" exists but it's associated with another node. No changed made. orig: %.*s new: %.*s\n:",
                      (int)alias_name_len, alias_name_str,
                      (int)SELVA_NODE_ID_SIZE, old_marker->node_id,
                      (int)SELVA_NODE_ID_SIZE, node_id);
        }

        /* Marker already created. */
        return SELVA_SUBSCRIPTIONS_EEXIST;
    }

    /*
     * Compile the filter.
     * `aliases has alias_name`
     */
    filter_expression = rpn_compile("$1 $2 a");
    if (!filter_expression) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to compile a filter for alias \"%s\"",
                  selva_string_to_str(alias_name, NULL));
        err = SELVA_RPN_ECOMP;
        goto fail;
    }

    filter_ctx = rpn_init(3);

    /*
     * Set RPN registers
     */
    enum rpn_error rpn_err;
    if ((rpn_err = rpn_set_reg_string(filter_ctx, 1, alias_name)) ||
        (rpn_err = rpn_set_reg(filter_ctx, 2, SELVA_ALIASES_FIELD, sizeof(SELVA_ALIASES_FIELD), 0))) {
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

        SELVA_LOG(SELVA_LOGL_ERR,
                  "Fatal RPN error while adding an alias maker. sub_id: %s alias: %s rpn_error: %d",
                  Selva_SubscriptionId2str(str, sub_id),
                  selva_string_to_str(alias_name, NULL),
                  rpn_err);
        if (rpn_err == RPN_ERR_ENOMEM) {
            err = SELVA_ENOMEM;
        } else {
            /* This is the closest we have until we merge RPN errors to SELVA errors. */
            err = SELVA_RPN_ECOMP;
        }
        goto fail;
    }

    struct Selva_SubscriptionMarker *marker;
    err = new_marker(hierarchy, sub_id, marker_id, NULL, 0, SELVA_SUBSCRIPTION_FLAG_CH_ALIAS, defer_update_event, &marker);
    if (err) {
        goto fail;
    }

    marker_set_node_id(marker, node_id);
    marker_set_dir(marker, SELVA_HIERARCHY_TRAVERSAL_NODE);
    marker_set_filter(marker, filter_ctx, filter_expression);

    return err;
fail:
    rpn_destroy(filter_ctx);
    rpn_destroy_expression(filter_expression);

    return err;
}

int SelvaSubscriptions_AddCallbackMarker(
        SelvaHierarchy *hierarchy,
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
    ) {
    struct rpn_expression *dir_expression = NULL;
    struct rpn_expression *filter = NULL;
    struct rpn_ctx *filter_ctx = NULL;
    struct Selva_SubscriptionMarker *marker;
    int err = 0;

    if (SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id)) {
        /* Marker already created. */
        return SELVA_SUBSCRIPTIONS_EEXIST;
    }

    if (dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION | SELVA_HIERARCHY_TRAVERSAL_EXPRESSION) && dir_expression_str) {
        dir_expression = rpn_compile(dir_expression_str);
        if (!dir_expression) {
            err = SELVA_RPN_ECOMP;
            goto out;
        }
    }

    if (filter_str) {
        filter = rpn_compile(filter_str);
        if (!filter) {
            err = SELVA_RPN_ECOMP;
            goto out;
        }

        filter_ctx = rpn_init(1);
    }

    /*
     * For now we just match to any field change and assume that the filter
     * takes care of the actual matching. This will work fine for indexing
     * but some other use cases might require another approach later on.
     */
    err = new_marker(hierarchy, sub_id, marker_id, filter ? "" : NULL, 0, marker_flags, callback, &marker);
    if (err) {
        goto out;
    }

    marker_set_node_id(marker, node_id);
    marker_set_dir(marker, dir);

    if (dir_expression) {
        marker_set_traversal_expression(marker, dir_expression);
    } else if (dir_field) {
        marker_set_ref_field(marker, dir_field);
    }

    if (filter) {
        marker_set_filter(marker, filter_ctx, filter);
    }

    marker_set_action_owner_ctx(marker, owner_ctx);

out:
    if (err) {
        rpn_destroy_expression(dir_expression);
        rpn_destroy_expression(filter);
        rpn_destroy(filter_ctx);
    }

    return err;
}

struct Selva_SubscriptionMarker *SelvaSubscriptions_GetMarker(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id) {
    struct Selva_Subscription *sub;

    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        return NULL;
    }

    return find_sub_marker(sub, marker_id);
}

/**
 * Do a traversal over the given marker.
 * Bear in mind that cb is passed directly to the hierarchy traversal, thus any
 * filter set in the marker is not executed and the callback must execute the
 * filter if required.
 */
static int SelvaSubscriptions_TraverseMarker(
        struct SelvaHierarchy *hierarchy,
        struct Selva_SubscriptionMarker *marker,
        SelvaHierarchyNodeCallback node_cb,
        void *node_arg) {
    int err = 0;
    typeof(marker->dir) dir = marker->dir;
    struct SelvaHierarchyCallback cb = {
        .node_cb = node_cb,
        .node_arg = node_arg,
    };

    /*
     * Some traversals don't visit the head node but the marker system must
     * always visit it.
     */
    if (dir &
        (SELVA_HIERARCHY_TRAVERSAL_REF |
         SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
         SELVA_HIERARCHY_TRAVERSAL_PARENTS |
         SELVA_HIERARCHY_TRAVERSAL_CHILDREN |
         SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD |
         SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) {
        cb.head_cb = node_cb;
        cb.head_arg = node_arg;
    }

    if (marker->ref_field &&
               (dir &
                (SELVA_HIERARCHY_TRAVERSAL_REF |
                 SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                 SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD))) {
        err = SelvaHierarchy_TraverseField(hierarchy, marker->node_id, dir, marker->ref_field, strlen(marker->ref_field), &cb);
    } else if (dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION | SELVA_HIERARCHY_TRAVERSAL_EXPRESSION) &&
               marker->traversal_expression) {
        struct rpn_ctx *rpn_ctx;

        rpn_ctx = rpn_init(1);
        if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
            err = SelvaHierarchy_TraverseExpressionBfs(hierarchy, marker->node_id, rpn_ctx, marker->traversal_expression, NULL, NULL, &cb);
        } else {
            err = SelvaHierarchy_TraverseExpression(hierarchy, marker->node_id, rpn_ctx, marker->traversal_expression, NULL, NULL, &cb);
        }
        rpn_destroy(rpn_ctx);
    } else {
        /*
         * The rest of the traversal directions are handled by the following
         * function.
         * We might also end up here when dir is one of the previous ones but
         * some other condition was false, or when dir is invalid. All possible
         * invalid cases will be handled propely by the following function.
         */
        err = SelvaHierarchy_Traverse(hierarchy, marker->node_id, dir, &cb);
    }
    if (err) {
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

        SELVA_LOG(SELVA_LOGL_DBG, "Could not fully apply a subscription marker: %s:%" PRImrkId " err: \"%s\"",
                  Selva_SubscriptionId2str(str, marker->sub->sub_id), marker->marker_id,
                  selva_strerror(err));

        /*
         * Don't report ENOENT errors because subscriptions are valid for
         * non-existent nodeIds.
         */
        if (err != SELVA_HIERARCHY_ENOENT) {
            return err;
        }
    }

    return 0;
}

static int refresh_marker(
        struct SelvaHierarchy *hierarchy,
        struct Selva_SubscriptionMarker *marker) {
    if (marker->dir == SELVA_HIERARCHY_TRAVERSAL_NONE ||
        (marker->marker_flags & SELVA_SUBSCRIPTION_FLAG_DETACH)) {
        /*
         * This is a non-traversing marker but it needs to exist in the
         * detached markers.
         */
        set_marker(&hierarchy->subs.detached_markers, marker);

        return 0;
    } else {
        struct set_node_marker_data cb_data = {
            .marker = marker,
        };

        /*
         * Set subscription markers.
         */
        return SelvaSubscriptions_TraverseMarker(hierarchy, marker, set_node_marker_cb, &cb_data);
    }
}

int SelvaSubscriptions_RefreshByMarkerId(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id) {
    struct Selva_SubscriptionMarker *marker;

    marker = SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id);
    if (!marker) {
        return SELVA_SUBSCRIPTIONS_ENOENT;
    }

    return refresh_marker(hierarchy, marker);
}

static int refreshSubscription(struct SelvaHierarchy *hierarchy, struct Selva_Subscription *sub) {
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;
    int res = 0;

    assert(sub);

    SVector_ForeachBegin(&it, &sub->markers);
    while ((marker = SVector_Foreach(&it))) {
        int err;

        err = refresh_marker(hierarchy, marker);
        if (err) {
            /* Report just the last error. */
            res = err;
        }
    }

    return res;
}

int SelvaSubscriptions_Refresh(struct SelvaHierarchy *hierarchy, const Selva_SubscriptionId sub_id) {
    struct Selva_Subscription *sub;

    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        return SELVA_SUBSCRIPTIONS_ENOENT;
    }

    return refreshSubscription(hierarchy, sub);
}

void SelvaSubscriptions_RefreshByMarker(struct SelvaHierarchy *hierarchy, const SVector *markers) {
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;

    SVector_ForeachBegin(&it, markers);
    while ((marker = SVector_Foreach(&it))) {
        /* Ignore errors for now. */
        (void)refreshSubscription(hierarchy, marker->sub);
    }
}

/**
 * Clear subscription starting from node_id.
 * Clear the given marker of a subscription from the nodes following traversal
 * direction starting from node_id.
 */
static void clear_node_sub(struct SelvaHierarchy *hierarchy, struct Selva_SubscriptionMarker *marker, const Selva_NodeId node_id) {
    struct SelvaHierarchyCallback cb = {
        .head_cb = clear_node_marker_cb,
        .head_arg = marker,
        .node_cb = clear_node_marker_cb,
        .node_arg = marker,
    };
    typeof(marker->dir) dir = marker->dir;

    /*
     * Remove subscription markers.
     */
    if (dir & (SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD | SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD)) {
        const char *ref_field_str = marker->ref_field;
        size_t ref_field_len = strlen(ref_field_str);

        (void)SelvaHierarchy_TraverseField(hierarchy, node_id, dir, ref_field_str, ref_field_len, &cb);
    } else if (dir &
               (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) {
        struct rpn_ctx *rpn_ctx;
        int err;
#if 0
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

        SELVA_LOG(SELVA_LOGL_DBG, "Clear sub marker %s:%" PRImrkId " from node %.*s",
                  Selva_SubscriptionId2str(str, marker->sub->sub_id),
                  marker->marker_id,
                  (int)SELVA_NODE_ID_SIZE, node_id);
#endif

        rpn_ctx = rpn_init(1);
        if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
            err = SelvaHierarchy_TraverseExpressionBfs(hierarchy, marker->node_id, rpn_ctx, marker->traversal_expression, NULL, NULL, &cb);
        } else {
            err = SelvaHierarchy_TraverseExpression(hierarchy, marker->node_id, rpn_ctx, marker->traversal_expression, NULL, NULL, &cb);
        }
        rpn_destroy(rpn_ctx);
        /* RFE SELVA_HIERARCHY_ENOENT is not good in case something was left but it's too late then */
        if (err && err != SELVA_HIERARCHY_ENOENT) {
            char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

            SELVA_LOG(SELVA_LOGL_ERR,
                      "Failed to clear a subscription %s:%" PRImrkId ": %s",
                      Selva_SubscriptionId2str(str, marker->sub->sub_id),
                      marker->marker_id,
                      selva_strerror(err));
            abort(); /* It would be dangerous to not abort here. */
        }
    } else {
        if (dir &
            (SELVA_HIERARCHY_TRAVERSAL_NONE |
             SELVA_HIERARCHY_TRAVERSAL_REF)) {
            dir = SELVA_HIERARCHY_TRAVERSAL_NODE;
        }

        (void)SelvaHierarchy_Traverse(hierarchy, node_id, dir, &cb);
    }
}

void SelvaSubscriptions_Delete(
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id) {
    struct Selva_Subscription *sub;

    sub = find_sub(hierarchy, sub_id);
    if (sub) {
        destroy_sub(hierarchy, sub);
    }
}

void SelvaSubscriptions_ClearAllMarkers(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node) {
    struct SelvaHierarchyMetadata *metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);
    const size_t nr_markers = SVector_Size(&metadata->sub_markers.vec);
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;
    SVECTOR_AUTOFREE(markers);
    Selva_NodeId node_id;

    SelvaHierarchy_GetNodeId(node_id, node);

    if (nr_markers == 0) {
        return;
    }

#if 0
    SELVA_LOG(SELVA_LOGL_DBG, "Removing %zu subscription markers from %.*s",
              nr_markers, (int)SELVA_NODE_ID_SIZE, node_id);
#endif

    if (unlikely(!SVector_Clone(&markers, &metadata->sub_markers.vec, NULL))) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to clone an SVector");
        return;
    }

    /*
     * Remove each subscription marker from this node and its ancestors/descendants.
     */
    SVector_ForeachBegin(&it, &markers);
    while ((marker = SVector_Foreach(&it))) {
        unsigned short flags = SELVA_SUBSCRIPTION_FLAG_CL_HIERARCHY | SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY;

        assert(marker->sub);
        clear_node_sub(hierarchy, marker, node_id);
        marker->marker_action(hierarchy, marker, flags, NULL, 0, node);
    }
    SVector_Clear(&metadata->sub_markers.vec);
}

void SelvaSubscriptions_DestroyDeferredEvents(struct SelvaHierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    if (!def) {
        return;
    }

    SVector_Destroy(&def->updates);
    SVector_Destroy(&def->triggers);
}

void SelvaSubscriptions_InheritParent(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id __unused,
        struct SelvaHierarchyMetadata *node_metadata,
        size_t node_nr_children,
        struct SelvaHierarchyNode *parent) {
    /*
     * Trigger all relevant subscriptions to make sure the subscriptions are
     * propagated properly.
     */
    if (node_nr_children > 0) {
        defer_event_for_traversing_markers(hierarchy, parent);
    } else {
        Selva_NodeId parent_id;
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;
        struct Selva_SubscriptionMarkers *node_sub_markers = &node_metadata->sub_markers;
        const SVector *markers_vec;

        SelvaHierarchy_GetNodeId(parent_id, parent);
        markers_vec = &SelvaHierarchy_GetNodeMetadataByPtr(parent)->sub_markers.vec;

        SVector_ForeachBegin(&it, markers_vec);
        while ((marker = SVector_Foreach(&it))) {
#if 0
            SELVA_LOG(SELVA_LOGL_DBG, "Inherit marker %d %.*s <- %.*s",
                    (int)marker->dir,
                    (int)SELVA_NODE_ID_SIZE, node_id,
                    (int)SELVA_NODE_ID_SIZE, parent_id);
#endif
            switch (marker->dir) {
            case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
            case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
            case SELVA_HIERARCHY_TRAVERSAL_DFS_FULL:
                /* These markers can be copied safely. */
                set_marker(node_sub_markers, marker);
                break;
            case SELVA_HIERARCHY_TRAVERSAL_CHILDREN:
                /* Only propagate if the parent is the first node. */
                if (!memcmp(parent_id, marker->node_id, SELVA_NODE_ID_SIZE)) {
                    set_marker(node_sub_markers, marker);
                }
                break;
            default:
                /*
                 * NOP.
                 */
                break;
            }
        }
    }
}

void SelvaSubscriptions_InheritChild(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id __unused,
        struct SelvaHierarchyMetadata *node_metadata,
        size_t node_nr_parents,
        struct SelvaHierarchyNode *child) {
    /*
     * Trigger all relevant subscriptions to make sure the subscriptions are
     * propagated properly.
     */
    if (node_nr_parents > 0) {
        defer_event_for_traversing_markers(hierarchy, child);
    } else {
        Selva_NodeId child_id;
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;
        struct Selva_SubscriptionMarkers *node_sub_markers;

        node_sub_markers = &node_metadata->sub_markers;
        SelvaHierarchy_GetNodeId(child_id, child);

        SVector_ForeachBegin(&it, &SelvaHierarchy_GetNodeMetadataByPtr(child)->sub_markers.vec);
        while ((marker = SVector_Foreach(&it))) {
            switch (marker->dir) {
            case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
            case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
                /* These markers can be copied safely. */
                set_marker(node_sub_markers, marker);
                break;
            case SELVA_HIERARCHY_TRAVERSAL_PARENTS:
                /* Only propagate if the child is the first node. */
                if (!memcmp(child_id, marker->node_id, SELVA_NODE_ID_SIZE)) {
                    set_marker(node_sub_markers, marker);
                }
                break;
            default:
                /*
                 * NOP.
                 */
                break;
            }
        }
    }
}

void SelvaSubscriptions_InheritEdge(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *src_node,
        struct SelvaHierarchyNode *dst_node,
        const char *field_str,
        size_t field_len) {
    Selva_NodeId src_node_id;
    Selva_NodeId dst_node_id;
    struct SelvaHierarchyMetadata *src_metadata = SelvaHierarchy_GetNodeMetadataByPtr(src_node);
    struct SelvaHierarchyMetadata *dst_metadata = SelvaHierarchy_GetNodeMetadataByPtr(dst_node);
    struct Selva_SubscriptionMarkers *src_markers = &src_metadata->sub_markers;
    struct Selva_SubscriptionMarkers *dst_markers = &dst_metadata->sub_markers;
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;
    int defer_all_traversing = 0;

    SelvaHierarchy_GetNodeId(src_node_id, src_node);
    SelvaHierarchy_GetNodeId(dst_node_id, dst_node);

    SVector_ForeachBegin(&it, &src_markers->vec);
    while ((marker = SVector_Foreach(&it))) {
        if ((marker->dir & SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD) ||
            ((marker->dir & SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD) && !memcmp(src_node_id, marker->node_id, SELVA_NODE_ID_SIZE))) {
            const size_t ref_field_len = strlen(marker->ref_field);

            if (field_len == ref_field_len && !strncmp(field_str, marker->ref_field, ref_field_len)) {
                set_marker(dst_markers, marker);

                if (!defer_all_traversing &&
                    (marker->dir & SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD) &&
                    Edge_GetField(dst_node, field_str, field_len)) {
                    /*
                     * If there was a traversing marker and the destination has the field
                     * too then we should send an event to make the client propagate the
                     * subscription marker by issuing a refresh.
                     * RFE Technically we could check whether the field is empty before
                     * doing this.
                     */
                    defer_all_traversing = 1;
                } else if ((marker->dir & SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD) &&
                           Selva_SubscriptionFilterMatch(hierarchy, dst_node, marker)) {
                    unsigned short flags = SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY;

                    /*
                     * In the case of a marker over single edge_field we should
                     * just trigger the markers that match.
                     */
                    marker->marker_action(hierarchy, marker, flags, NULL, 0, dst_node);
                }
            }
        }
    }

    if (defer_all_traversing) {
        defer_event_for_traversing_markers(hierarchy, dst_node);
    }
}

static void defer_update_event(
        struct SelvaHierarchy *hierarchy,
        struct Selva_SubscriptionMarker *marker,
        unsigned short event_flags __unused,
        const char *field_str __unused,
        size_t field_len __unused,
        struct SelvaHierarchyNode *node __unused) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    struct Selva_Subscription *sub = marker->sub;

    if (SVector_IsInitialized(&def->updates)) {
        SVector_InsertFast(&def->updates, sub);
    }
}

static void defer_trigger_event(
        struct SelvaHierarchy *hierarchy,
        struct Selva_SubscriptionMarker *marker,
        unsigned short event_flags __unused,
        const char *field_str __unused,
        size_t field_len __unused,
        struct SelvaHierarchyNode *node __unused) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    if (SVector_IsInitialized(&def->triggers)) {
        SVector_InsertFast(&def->triggers, marker);
    }
}

/**
 * Defer events for missing accessor signaling creation of nodes and aliases.
 * @param id nodeId or alias.
 */
void SelvaSubscriptions_DeferMissingAccessorEvents(struct SelvaHierarchy *hierarchy, const char *id_str, size_t id_len) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    struct SelvaObject *missing = GET_STATIC_SELVA_OBJECT(&hierarchy->subs.missing);
    struct Selva_Subscription *sub;
    SelvaObject_Iterator *it;
    struct SelvaObject *obj;
    int err;

    /* Get the <id> object containing a number of subscription pointers for this id. */
    err = SelvaObject_GetObjectStr(missing, id_str, id_len, &obj);
    if (err || !obj) {
        if (err == SELVA_ENOENT || (!err && !obj)) {
            return;
        }

        SELVA_LOG(SELVA_LOGL_ERR, "Failed to get missing accessor marker: %s",
                  selva_strerror(err));
        return;
    }

    /* Defer event for each subscription. */
    it = SelvaObject_ForeachBegin(obj);
    while ((sub = SelvaObject_ForeachValue(obj, &it, NULL, SELVA_OBJECT_POINTER))) {
        /*
         * These are a bit special because we have no marker structs but the
         * markers are pointers to the subscription in a SelvaObject.
         * The event type is an update.
         */
        SVector_InsertFast(&def->updates, sub);
    }

    /* Finally delete the ID as all events were deferred. */
    SelvaObject_DelKeyStr(missing, id_str, id_len);
}

/**
 * Defer event if a marker is traversing marker.
 * Use defer_event_for_traversing_markers() instead of this function.
 */
static void defer_traversing(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct Selva_SubscriptionMarkers *sub_markers) {
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;

    SVector_ForeachBegin(&it, &sub_markers->vec);
    while ((marker = SVector_Foreach(&it))) {
        if (marker->dir != SELVA_HIERARCHY_TRAVERSAL_NONE) {
            unsigned short flags = SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY;

            marker->marker_action(hierarchy, marker, flags, NULL, 0, node);
        }
    }
}

static void defer_event_for_traversing_markers(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node) {
    /* Detached markers. */
    defer_traversing(hierarchy, node, &hierarchy->subs.detached_markers);

    /* Markers on the node. */
    defer_traversing(hierarchy, node, &SelvaHierarchy_GetNodeMetadataByPtr(node)->sub_markers);
}

static void defer_hierarchy_events(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct Selva_SubscriptionMarkers *sub_markers) {
    if (isHierarchyMarker(sub_markers->flags_filter)) {
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;

        SVector_ForeachBegin(&it, &sub_markers->vec);
        while ((marker = SVector_Foreach(&it))) {
            if (isHierarchyMarker(marker->marker_flags) &&
                Selva_SubscriptionFilterMatch(hierarchy, node, marker)) {
                unsigned short flags = SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY;

                marker->marker_action(hierarchy, marker, flags, NULL, 0, node);
            }
        }
    }
}

void SelvaSubscriptions_DeferHierarchyEvents(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node) {
    const struct SelvaHierarchyMetadata *metadata;

    /* Detached markers. */
    defer_hierarchy_events(hierarchy, node, &hierarchy->subs.detached_markers);

    /* Markers on the node. */
    metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);
    defer_hierarchy_events(hierarchy, node, &metadata->sub_markers);
}

static void defer_hierarchy_deletion_events(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct Selva_SubscriptionMarkers *sub_markers) {
    if (isHierarchyMarker(sub_markers->flags_filter)) {
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;

        SVector_ForeachBegin(&it, &sub_markers->vec);
        while ((marker = SVector_Foreach(&it))) {
            /*
             * Deletions are always sent to all hierarchy markers regardless of
             * field subscriptions or inhibits.
             */
            if (isHierarchyMarker(marker->marker_flags)) {
                unsigned short flags = SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY;

                marker->marker_action(hierarchy, marker, flags, NULL, 0, node);
            }
        }
    }
}

void SelvaSubscriptions_DeferHierarchyDeletionEvents(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node) {
    const struct SelvaHierarchyMetadata *metadata;

    /* Detached markers. */
    defer_hierarchy_deletion_events(hierarchy, node, &hierarchy->subs.detached_markers);

    /* Markers on the node. */
    metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);
    defer_hierarchy_deletion_events(hierarchy, node, &metadata->sub_markers);
}

static void defer_alias_change_events(
        struct SelvaHierarchy *hierarchy,
        const struct Selva_SubscriptionMarkers *sub_markers,
        const Selva_NodeId node_id,
        SVector *wipe_subs) {
    struct SelvaHierarchyNode *node;
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;

    if (!isAliasMarker(sub_markers->flags_filter)) {
        /* No alias markers in this structure. */
        return;
    }

    node = SelvaHierarchy_FindNode(hierarchy, node_id);

    SVector_ForeachBegin(&it, &sub_markers->vec);
    while ((marker = SVector_Foreach(&it))) {
        if (isAliasMarker(marker->marker_flags) &&
            /* The filter should contain `in` matcher for the alias. */
            Selva_SubscriptionFilterMatch(hierarchy, node, marker)
            ) {
            unsigned short flags = SELVA_SUBSCRIPTION_FLAG_CH_ALIAS;

            marker->marker_action(hierarchy, marker, flags, NULL, 0, node);

            /*
             * Wipe the markers of this subscription after the events have been
             * deferred.
             */
            SVector_Insert(wipe_subs, marker->sub);
        }
    }
}

/**
 * Check whether the filter matches before changing the value of a node field.
 */
static void field_change_precheck(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct Selva_SubscriptionMarkers *sub_markers) {
    const unsigned short flags = SELVA_SUBSCRIPTION_FLAG_CH_FIELD;

    if ((sub_markers->flags_filter & flags) == flags) {
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;

        SVector_ForeachBegin(&it, &sub_markers->vec);
        while ((marker = SVector_Foreach(&it))) {
            if ((marker->marker_flags & flags) == flags) {
                /*
                 * Store the filter result before any changes to the node.
                 * We assume that SelvaSubscriptions_DeferFieldChangeEvents()
                 * is called before this function is called for another node.
                 */
                SelvaHierarchy_GetNodeId(marker->filter_history.node_id, node);
                marker->filter_history.res = Selva_SubscriptionFilterMatch(hierarchy, node, marker);
            }
        }
    }
}

void SelvaSubscriptions_FieldChangePrecheck(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node) {
    const struct SelvaHierarchyMetadata *metadata;

    /* Detached markers. */
    field_change_precheck(hierarchy, node, &hierarchy->subs.detached_markers);

    /* Markers on the node. */
    metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);
    field_change_precheck(hierarchy, node, &metadata->sub_markers);
}

static void defer_field_change_events(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct Selva_SubscriptionMarkers *sub_markers,
        const char *field_str,
        size_t field_len) {
    const unsigned short flags = SELVA_SUBSCRIPTION_FLAG_CH_FIELD;

    if ((sub_markers->flags_filter & flags) == flags) {
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;

        SVector_ForeachBegin(&it, &sub_markers->vec);
        while ((marker = SVector_Foreach(&it))) {
            Selva_NodeId node_id;

            SelvaHierarchy_GetNodeId(node_id, node);

            if (((marker->marker_flags & flags) == flags) && !inhibitMarkerEvent(node_id, marker)) {
                const int expressionMatchBefore = marker->filter_history.res && !memcmp(marker->filter_history.node_id, node_id, SELVA_NODE_ID_SIZE);
                const int expressionMatchAfter = Selva_SubscriptionFilterMatch(hierarchy, node, marker);
                const int fieldsMatch = Selva_SubscriptionFieldMatch(marker, field_str, field_len);

                if ((expressionMatchBefore && expressionMatchAfter && fieldsMatch) || (expressionMatchBefore ^ expressionMatchAfter)) {
                    marker->marker_action(hierarchy, marker, flags, field_str, field_len, node);
                }
            }
        }
    }
}

static void defer_array_field_change_events(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct Selva_SubscriptionMarkers *sub_markers,
        const char *field_str,
        size_t field_len) {
    const char *ary_field_start = (const char *)memrchr(field_str, '[', field_len);
    ssize_t ary_field_len;

    if (ary_field_start) {
        ary_field_len = ary_field_start - field_str;
    } else {
        ary_field_len = -1;
    }

    if (ary_field_len > 0) {
        char ary_field_str[ary_field_len + 1];
        int path_field_len = -1;
        const char *path_field_start = NULL;

        path_field_start = (const char *)memrchr(field_str + ary_field_len, ']', field_len - ary_field_len);
        if (path_field_start) {
            path_field_start++;
            /* path part */
            path_field_len = field_len - (path_field_start - field_str);

            /* array field part */
            path_field_len += ary_field_len;

            /* [n] part */
            path_field_len += 3;
        }

        if (path_field_start && *path_field_start != '\0') {
            char path_field_str[path_field_len + 1];

            snprintf(path_field_str, path_field_len + 1, "%.*s[n]%s", (int)ary_field_len, field_str, path_field_start);
            defer_field_change_events(hierarchy, node, sub_markers, path_field_str, path_field_len);
        }

        memcpy(ary_field_str, field_str, ary_field_len);
        ary_field_str[ary_field_len] = '\0';
        /* check for direct subscriptions on arrayField: true */
        defer_field_change_events(hierarchy, node, sub_markers, ary_field_str, ary_field_len);
    }
}

void SelvaSubscriptions_DeferFieldChangeEvents(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len) {
    if (memrchr(field_str, '[', field_len)) {
        /* Array */
        /* Detached markers. */
        defer_array_field_change_events(hierarchy, node, &hierarchy->subs.detached_markers, field_str, field_len);

        const struct SelvaHierarchyMetadata *metadata;
        metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);

        /* Markers on the node. */
        defer_array_field_change_events(hierarchy, node, &metadata->sub_markers, field_str, field_len);
    } else {
        /* Regular field */
        /* Detached markers. */
        defer_field_change_events(hierarchy, node, &hierarchy->subs.detached_markers, field_str, field_len);

        const struct SelvaHierarchyMetadata *metadata;
        metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);

        /* Markers on the node. */
        defer_field_change_events(hierarchy, node, &metadata->sub_markers, field_str, field_len);
    }
}

/**
 * Defer alias events and wipeout markers of the subscriptions hit.
 */
void SelvaSubscriptions_DeferAliasChangeEvents(
        struct SelvaHierarchy *hierarchy,
        struct selva_string *alias_name) {
    SVECTOR_AUTOFREE(wipe_subs);
    Selva_NodeId orig_node_id;
    struct SelvaHierarchyMetadata *orig_metadata;
    int err;

    SVector_Init(&wipe_subs, 0, SelvaSubscription_svector_compare);

    err = SelvaResolve_NodeId(hierarchy, (struct selva_string *[]){ alias_name }, 1, orig_node_id);
    if (err < 0) {
        return;
    }

    orig_metadata = SelvaHierarchy_GetNodeMetadata(hierarchy, orig_node_id);
    if (!orig_metadata) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to get metadata for node: \"%.*s\"",
                  (int)SELVA_NODE_ID_SIZE, orig_node_id);
        return;
    }

    /*
     * Alias markers are never detached so no need to handle those.
     */

    /* Defer events for markers on the src node. */
    defer_alias_change_events(
            hierarchy,
            &orig_metadata->sub_markers,
            orig_node_id,
            &wipe_subs);

    struct SVectorIterator it;
    struct Selva_Subscription *sub;

    /* Wipe all markers of the subscriptions that were hit. */
    SVector_ForeachBegin(&it, &wipe_subs);
    while ((sub = SVector_Foreach(&it))) {
        remove_sub_markers(hierarchy, sub);
    }
}

void SelvaSubscriptions_DeferTriggerEvents(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        enum Selva_SubscriptionTriggerType event_type) {
    /* Trigger markers are always detached and have no node_id. */
    const struct Selva_SubscriptionMarkers *sub_markers = &hierarchy->subs.detached_markers;

    if (isTriggerMarker(sub_markers->flags_filter)) {
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;

        SVector_ForeachBegin(&it, &sub_markers->vec);
        while ((marker = SVector_Foreach(&it))) {
            if (isTriggerMarker(marker->marker_flags) &&
                marker->event_type == event_type &&
                Selva_SubscriptionFilterMatch(hierarchy, node, marker)) {
                unsigned short flags = SELVA_SUBSCRIPTION_FLAG_TRIGGER;

                /*
                 * The node_id might be there already if the marker has a filter
                 * but trigger events will need the node_id there regardless of if
                 * a filter is actually used.
                 */
                SelvaHierarchy_GetNodeId(marker->filter_history.node_id, node);

                /*
                 * We don't call defer_trigger_event() here directly to allow
                 * customization of subscription marker events.
                 * Note that the node pointer is only valid during this function call.
                 */
                marker->marker_action(hierarchy, marker, flags, NULL, 0, node);
            }
        }
    }
}

static void send_update_events(struct SelvaHierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    struct SVectorIterator it;
    const struct Selva_Subscription *sub;

    SVector_ForeachBegin(&it, &def->updates);
    while ((sub = SVector_Foreach(&it))) {
        struct SelvaSubscriptions_PubsubMessage msg = {
            .event_type = SELVA_SUB_UPDATE,
        };

        memcpy(msg.sub_id, sub->sub_id, SELVA_SUBSCRIPTION_ID_SIZE);
#if 0
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

        SELVA_LOG(SELVA_LOGL_DBG, "Publish update event %s",
                  Selva_SubscriptionId2str(str, sub->sub_id));
#endif

        selva_pubsub_publish(SELVA_SUBSCRIPTIONS_PUBSUB_CH_ID, &msg, sizeof(msg));
    }
    SVector_Clear(&def->updates);
}

static void send_trigger_events(struct SelvaHierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    struct SVectorIterator it;
    const struct Selva_SubscriptionMarker *marker;

    SVector_ForeachBegin(&it, &def->triggers);
    while ((marker = SVector_Foreach(&it))) {
        struct SelvaSubscriptions_PubsubMessage msg = {
            .event_type = SELVA_SUB_TRIGGER,
        };

        memcpy(msg.sub_id, marker->sub->sub_id, SELVA_SUBSCRIPTION_ID_SIZE);
        memcpy(msg.node_id, marker->filter_history.node_id, SELVA_NODE_ID_SIZE);
#if 0
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

        SELVA_LOG(SELVA_LOGL_DBG, "Publish trigger event %s",
                  Selva_SubscriptionId2str(str, marker->sub->sub_id));
#endif

        selva_pubsub_publish(SELVA_SUBSCRIPTIONS_PUBSUB_CH_ID, &msg, sizeof(msg));
    }
    SVector_Clear(&def->triggers);
}

void SelvaSubscriptions_SendDeferredEvents(struct SelvaHierarchy *hierarchy) {
    send_update_events(hierarchy);
    send_trigger_events(hierarchy);
}

void SelvaSubscriptions_ReplyWithMarker(struct selva_server_response_out *resp, struct Selva_SubscriptionMarker *marker) {
    char sub_buf[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
    const int is_trigger = isTriggerMarker(marker->marker_flags);

    selva_send_array(resp, -1);
    selva_send_strf(resp, "sub_id: %s", Selva_SubscriptionId2str(sub_buf, marker->sub->sub_id));
    selva_send_strf(resp, "marker_id: %" PRImrkId, marker->marker_id);
    selva_send_strf(resp, "flags: 0x%04x", marker->marker_flags);
    if (is_trigger) {
        selva_send_strf(resp, "event_type: %s", trigger_event_types[marker->event_type].name);
    } else {
        selva_send_strf(resp, "node_id: \"%.*s\"", (int)SELVA_NODE_ID_SIZE, marker->node_id);
        selva_send_strf(resp, "dir: %s", SelvaTraversal_Dir2str(marker->dir));

        if (marker->dir & (SELVA_HIERARCHY_TRAVERSAL_REF | SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD | SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD)) {
            selva_send_strf(resp, "field: %s", marker->ref_field);
        }
    }
    selva_send_strf(resp, "filter_expression: %s", (marker->filter_ctx) ? "set" : "unset");
    if (!is_trigger && (marker->marker_flags & SELVA_SUBSCRIPTION_FLAG_CH_FIELD)) {
        selva_send_strf(resp, "fields: \"%s\"", marker->fields);
    }

    selva_send_array_end(resp);
}

/*
 * Add a new marker to the subscription.
 * KEY SUB_ID MARKER_ID traversal_type [ref_field_name] NODE_ID [fields <fieldnames \n separated>] [filter expression] [filter args...]
 */
void SelvaSubscriptions_AddMarkerCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    int err;

    finalizer_init(&fin);

    const int ARGV_SUB_ID        = 0;
    const int ARGV_MARKER_ID     = 1;
    const int ARGV_MARKER_DIR    = 2;
    const int ARGV_REF_FIELD     = 3;
    int ARGV_NODE_ID             = 3;
    int ARGV_FIELDS              = 4;
    int ARGV_FIELD_NAMES         = 5;
    int ARGV_FILTER_EXPR         = 4;
    int ARGV_FILTER_ARGS         = 5;
#define SHIFT_ARGS(i) \
    ARGV_NODE_ID += i; \
    ARGV_FIELDS += i; \
    ARGV_FIELD_NAMES += i; \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    } else if (argc < ARGV_NODE_ID + 1) {
        selva_send_error_arity(resp);
        return;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = Selva_SubscriptionString2id(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        selva_send_errorf(resp, err, "Subscription ID");
        return;
    }

    /*
     * Get the marker id.
     */
    long long ll;
    Selva_SubscriptionMarkerId marker_id;
    err = selva_string_to_ll(argv[ARGV_MARKER_ID], &ll);
    if (err) {
        selva_send_errorf(resp, err, "Marker ID");
        return;
    }
    marker_id = ll;

    if (SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id)) {
        /* Marker already created. */
        selva_send_ll(resp, 1);
        return;
    }

    /*
     * Parse the traversal argument.
     */
    enum SelvaTraversal sub_dir;
    const char *ref_field = NULL;
    err = SelvaTraversal_ParseDir2(&sub_dir, argv[ARGV_MARKER_DIR]);
    if (err ||
        !(sub_dir &
          (SELVA_HIERARCHY_TRAVERSAL_NONE | SELVA_HIERARCHY_TRAVERSAL_NODE | SELVA_HIERARCHY_TRAVERSAL_CHILDREN |
           SELVA_HIERARCHY_TRAVERSAL_PARENTS | SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS | SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS |
           SELVA_HIERARCHY_TRAVERSAL_REF | SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD | SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD |
           SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION | SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)
        )) {
        selva_send_errorf(resp, err, "Traversal argument");
        return;
    }

    if (sub_dir & (SELVA_HIERARCHY_TRAVERSAL_REF | SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD | SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD)) {
        ref_field = selva_string_to_str(argv[ARGV_REF_FIELD], NULL);
        SHIFT_ARGS(1);
    }

    struct rpn_expression *traversal_expression = NULL;
    struct rpn_ctx *filter_ctx = NULL;
    struct rpn_expression *filter_expression = NULL;
    if (sub_dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION | SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) {
        const struct selva_string *input = argv[ARGV_REF_FIELD];
        TO_STR(input);

        traversal_expression = rpn_compile(input_str);
        if (!traversal_expression) {
            err = SELVA_RPN_ECOMP;
            selva_send_errorf(resp, err, "Failed to compile the traversal expression");
            goto out;
        }
        SHIFT_ARGS(1);
    }

    /*
     * Get the nodeId.
     */
    Selva_NodeId node_id;
    err = selva_string2node_id(node_id, argv[ARGV_NODE_ID]);
    if (err) {
        selva_send_errorf(resp, err, "node_id");
        return;
    }

    /*
     * Get field names for change events.
     * Optional.
     */
    const char *fields_str = NULL;
    size_t fields_len = 0;
    if (argc > ARGV_FIELD_NAMES) {
        err = SelvaArgParser_StrOpt(NULL, "fields", argv[ARGV_FIELDS], argv[ARGV_FIELD_NAMES]);
        if (err == 0) {
            fields_str = selva_string_to_str(argv[ARGV_FIELD_NAMES], &fields_len);
            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            selva_send_errorf(resp, err, "Fields");
            goto out;
        }
    }

    /*
     * Parse & compile the filter expression.
     * Optional.
     */
    if (argc >= ARGV_FILTER_EXPR + 1) {
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
        const char *input;
        size_t input_len;

        filter_ctx = rpn_init(nr_reg);

        /*
         * Compile the filter expression.
         */
        input = selva_string_to_str(argv[ARGV_FILTER_EXPR], &input_len);
        filter_expression = rpn_compile(input);
        if (!filter_expression) {
            err = SELVA_RPN_ECOMP;
            selva_send_errorf(resp, err, "Failed to compile the traversal expression");
            goto out;
        }

        /*
         * Get the filter expression arguments and set them to the registers.
         */
        for (int i = ARGV_FILTER_ARGS; i < argc; i++) {
            /* reg[0] is reserved for the current nodeId */
            const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
            size_t str_len;
            const char *str;
            char *arg;

            /*
             * Args needs to be duplicated so the strings don't get freed
             * when the command returns.
             */
            str = selva_string_to_str(argv[i], &str_len);
            str_len++;
            arg = selva_malloc(str_len);
            memcpy(arg, str, str_len);

            rpn_set_reg(filter_ctx, reg_i, arg, str_len, RPN_SET_REG_FLAG_SELVA_FREE);
        }
    }

    unsigned short marker_flags = 0;

    if (sub_dir & (SELVA_HIERARCHY_TRAVERSAL_CHILDREN | SELVA_HIERARCHY_TRAVERSAL_PARENTS)) {
        /*
         * RFE We might want to have an arg for REF flag
         * but currently it seems to be enough to support
         * it only for these specific traversal types.
         */
        marker_flags = SELVA_SUBSCRIPTION_FLAG_REF;
    }

    struct Selva_SubscriptionMarker *marker;
    err = new_marker(hierarchy, sub_id, marker_id, fields_str, fields_len, marker_flags, defer_update_event, &marker);
    if (err) {
        if (err == SELVA_SUBSCRIPTIONS_EEXIST) {
            /* This shouldn't happen as we check for this already before. */
            if (traversal_expression) {
                rpn_destroy_expression(traversal_expression);
            }
            if (filter_ctx) {
                rpn_destroy(filter_ctx);
                rpn_destroy_expression(filter_expression);
            }

            selva_send_ll(resp, 1);
            return;
        }

        selva_send_errorf(resp, err, "Failed to create a subscription");
        goto out;
    }

    marker_set_node_id(marker, node_id);
    marker_set_dir(marker, sub_dir);
    if (ref_field) {
        marker_set_ref_field(marker, ref_field);
    }
    if (traversal_expression) {
        marker_set_traversal_expression(marker, traversal_expression);
    }

    marker_set_filter(marker, filter_ctx, filter_expression);

out:
    if (err) {
        if (traversal_expression) {
            rpn_destroy_expression(traversal_expression);
        }
        if (filter_ctx) {
            rpn_destroy(filter_ctx);
            rpn_destroy_expression(filter_expression);
        }
    } else {
        selva_send_ll(resp, 1);
    }
#undef SHIFT_ARGS
}

/*
 * SUB_ID MARKER_ID ALIAS_NAME
 */
void SelvaSubscriptions_AddAliasCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    __auto_finalizer struct finalizer fin;
    const char *sub_id_str;
    size_t sub_id_len;
    Selva_SubscriptionMarkerId marker_id;
    struct selva_string *alias_name;
    int argc;
    int err;

    finalizer_init(&fin);

    argc = selva_proto_scanf(&fin, buf, len, "%.*s, %" PRImrkId ", %p",
                             &sub_id_len, &sub_id_str,
                             &marker_id,
                             &alias_name);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    } else if (argc != 3) {
        selva_send_error_arity(resp);
        return;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    if (sub_id_len == SELVA_SUBSCRIPTION_ID_SIZE) {
        memcpy(sub_id, sub_id_str, SELVA_SUBSCRIPTION_ID_SIZE);
    } else {
        err = Selva_SubscriptionStr2id(sub_id, sub_id_str, sub_id_len);
        if (err) {
            selva_send_errorf(resp, err, "Subscription ID");
            return;
        }
    }

    /*
     * Resolve the node_id as we want to apply the marker
     * on the node the alias is pointing to.
     */
    Selva_NodeId node_id;
    struct selva_string *aliases[] = { alias_name };
    err = SelvaResolve_NodeId(hierarchy, aliases, num_elem(aliases), node_id);
    if (err < 0) {
        selva_send_error(resp, err, NULL, 0);
        return;
    }

    err = Selva_AddSubscriptionAliasMarker(hierarchy, sub_id, marker_id, alias_name, node_id);
    if (err) {
        selva_send_error(resp, err, NULL, 0);
    } else {
        selva_send_ll(resp, 1);
    }
}

/**
 * Add missing node/alias markers.
 * SUB_ID NODEID|ALIAS...
 */
void SelvaSubscriptions_AddMissingCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    int err;

    finalizer_init(&fin);

    const int ARGV_SUB_ID    = 0;
    const int ARGV_IDS       = 1;

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc < 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    Selva_SubscriptionId sub_id;
    err = Selva_SubscriptionString2id(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        selva_send_errorf(resp, err, "Invalid Subscription ID");
        return;
    }

    /*
     * Open the subscription.
     */
    struct Selva_Subscription *sub;
    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        sub = create_subscription(hierarchy, sub_id);
        if (!sub) {
            selva_send_error(resp, SELVA_SUBSCRIPTIONS_EINVAL, NULL, 0);
            return;
        }
    }

    struct SelvaObject *missing = GET_STATIC_SELVA_OBJECT(&hierarchy->subs.missing);
    long long n = 0;
    for (int i = ARGV_IDS; i < argc; i++) {
        Selva_NodeId resolved_node_id;

        if (SelvaResolve_NodeId(hierarchy, (struct selva_string *[]){ argv[i] }, 1, resolved_node_id) > 0) {
            /*
             * Node exists.
             * Note that the subscription might have been created anyway.
             */
            continue;
        }

        size_t arg_len;
        const char *arg_str = selva_string_to_str(argv[i], &arg_len);
        size_t key_len = arg_len + 1 + SELVA_SUBSCRIPTION_ID_STR_LEN;
        char key_str[key_len + 1];

        snprintf(key_str, arg_len + 2, "%s.", arg_str);
        Selva_SubscriptionId2str(key_str + arg_len + 1, sub_id);
        key_str[key_len] = '\0';

        /* We don't need to care if it was already set. */
        err = SelvaObject_SetPointerStr(missing, key_str, key_len, sub, &subs_missing_obj_opts);
        if (err) {
            selva_send_error(resp, err, NULL, 0);
            return;
        }

        n++;
    }

    selva_send_ll(resp, n);
}

/**
 * Add a trigger marker.
 * SUBSCRIPTIONS.ADDTRIGGER SUB_ID MARKER_ID EVENT_TYPE [filter expression] [filter args...]
 */
void SelvaSubscriptions_AddTriggerCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv;
    int argc;
    int err;

    finalizer_init(&fin);

    const int ARGV_SUB_ID        = 0;
    const int ARGV_MARKER_ID     = 1;
    const int ARGV_EVENT_TYPE    = 2;
    int ARGV_FILTER_EXPR         = 3;
    int ARGV_FILTER_ARGS         = 4;

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    } else if (argc < ARGV_EVENT_TYPE + 1) {
        selva_send_error_arity(resp);
        return;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = Selva_SubscriptionString2id(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        selva_send_errorf(resp, err, "Subscription ID");
        return;
    }

    /*
     * Get the marker id.
     */
    long long ll;
    Selva_SubscriptionMarkerId marker_id;
    err = selva_string_to_ll(argv[ARGV_MARKER_ID], &ll);
    if (err) {
        selva_send_errorf(resp, err, "Marker ID");
        return;
    }
    marker_id = ll;

    /* Parse event_type */
    err = SelvaArgParser_Enum(trigger_event_types, argv[ARGV_EVENT_TYPE]);
    if (err < 0) {
        selva_send_errorf(resp, err, "Event type");
        return;
    }
    const enum Selva_SubscriptionTriggerType event_type = err;

    if (SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id)) {
        /* Marker already created. */
        selva_send_ll(resp, 1);
        return;
    }

    /*
     * Parse & compile the filter expression.
     * Optional.
     */
    struct rpn_ctx *filter_ctx = NULL;
    struct rpn_expression *filter_expression = NULL;
    if (argc >= ARGV_FILTER_EXPR + 1) {
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
        const char *input;
        size_t input_len;

        filter_ctx = rpn_init(nr_reg);

        /*
         * Compile the filter expression.
         */
        input = selva_string_to_str(argv[ARGV_FILTER_EXPR], &input_len);
        filter_expression = rpn_compile(input);
        if (!filter_expression) {
            err = SELVA_RPN_ECOMP;
            selva_send_errorf(resp, err, "Failed to compile a filter expression");
            goto out;
        }

        /*
         * Get the filter expression arguments and set them to the registers.
         */
        for (int i = ARGV_FILTER_ARGS; i < argc; i++) {
            /* reg[0] is reserved for the current nodeId */
            const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
            size_t str_len;
            const char *str;
            char *arg;

            /*
             * Args needs to be duplicated so the strings don't get freed
             * when the command returns.
             */
            str = selva_string_to_str(argv[i], &str_len);
            str_len++;
            arg = selva_malloc(str_len);
            memcpy(arg, str, str_len);

            rpn_set_reg(filter_ctx, reg_i, arg, str_len, RPN_SET_REG_FLAG_SELVA_FREE);
        }
    }

    const unsigned short marker_flags = SELVA_SUBSCRIPTION_FLAG_DETACH | SELVA_SUBSCRIPTION_FLAG_TRIGGER;
    struct Selva_SubscriptionMarker *marker;

    /*
     * Trigger never checks fields.
     */
    err = new_marker(hierarchy, sub_id, marker_id, NULL, 0, marker_flags, defer_trigger_event, &marker);
    if (err) {
        if (err == SELVA_SUBSCRIPTIONS_EEXIST) {
            /* This shouldn't happen as we check for this already before. */
            rpn_destroy(filter_ctx);
            rpn_destroy_expression(filter_expression);

            selva_send_ll(resp, 1);
            return;
        }

        selva_send_errorf(resp, err, "Failed to create a subscription");
        goto out;
    }

    marker_set_trigger(marker, event_type);
    marker_set_filter(marker, filter_ctx, filter_expression);

out:
    if (err) {
        if (filter_ctx) {
            rpn_destroy(filter_ctx);
            rpn_destroy_expression(filter_expression);
        }
    } else {
        selva_send_ll(resp, 1);
    }
}

/*
 * SUBSCRIPTIONS.refresh SUB_ID
 */
void SelvaSubscriptions_RefreshCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SELVA_TRACE_BEGIN_AUTO(cmd_subscriptions_refresh);
    SelvaHierarchy *hierarchy = main_hierarchy;
    const char *sub_id_str;
    size_t sub_id_len;
    int argc, err;

    argc = selva_proto_scanf(NULL, buf, len, "%.*s", &sub_id_len, &sub_id_str);
    if (argc != 1) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    Selva_SubscriptionId sub_id;
    err = Selva_SubscriptionStr2id(sub_id, sub_id_str, sub_id_len);
    if (err) {
        selva_send_errorf(resp, err, "Subscription ID");
        return;
    }

    struct Selva_Subscription *sub;
    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        selva_send_error(resp, SELVA_SUBSCRIPTIONS_ENOENT, NULL, 0);
        return;
    }

    err = refreshSubscription(hierarchy, sub);
    if (err) {
        selva_send_error(resp, err, NULL, 0);
    } else {
        selva_send_ll(resp, 1);
    }
}

static void Selva_Subscription_reply(struct selva_server_response_out *resp, void *p)
{
    struct Selva_Subscription *sub = p;
    char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

    selva_send_array(resp, 2);
    selva_send_str(resp, Selva_SubscriptionId2str(str, sub->sub_id), SELVA_SUBSCRIPTION_ID_STR_LEN);
    selva_send_ll(resp, SVector_Size(&sub->markers));
}

/**
 * List all subscriptions.
 */
void SelvaSubscriptions_ListCommand(struct selva_server_response_out *resp, const void *buf __unused, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    struct Selva_Subscription *sub;

    if (len != 0) {
        selva_send_error_arity(resp);
        return;
    }

    selva_send_array(resp, -1);

    RB_FOREACH(sub, hierarchy_subscriptions_tree, &hierarchy->subs.head) {
        Selva_Subscription_reply(resp, sub);
    }

    selva_send_array_end(resp);
}

void SelvaSubscriptions_ListMissingCommand(struct selva_server_response_out *resp, const void *buf __unused, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    struct SelvaObject *missing = GET_STATIC_SELVA_OBJECT(&hierarchy->subs.missing);
    int err;

    if (len != 0) {
        selva_send_error_arity(resp);
        return;
    }

    err = SelvaObject_ReplyWithObject(resp, NULL, missing, NULL, 0);
    if (err) {
        selva_send_error(resp, err, NULL, 0);
    }
}

/*
 * KEY SUB_ID
 */
void SelvaSubscriptions_DebugCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    const char *id_str;
    size_t id_len;
    int argc;

    argc = selva_proto_scanf(NULL, buf, len, "%.*s", &id_len, &id_str);
    if (argc != 1) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    const int is_sub_id = id_len == SELVA_SUBSCRIPTION_ID_STR_LEN;
    const int is_node_id = id_len <= SELVA_NODE_ID_SIZE;
    SVector *markers = NULL;

    if (is_sub_id) {
        Selva_SubscriptionId sub_id;
        struct Selva_Subscription *sub;
        int err;

        err = Selva_SubscriptionStr2id(sub_id, id_str, id_len);
        if (err) {
            selva_send_errorf(resp, err, "Subscription ID");
            return;
        }

        sub = find_sub(hierarchy, sub_id);
        if (!sub) {
            selva_send_error(resp, SELVA_SUBSCRIPTIONS_ENOENT, NULL, 0);
            return;
        }

        markers = &sub->markers;
    } else if (is_node_id) {
        if (id_len == (sizeof("detached") - 1) && !memcmp("detached", id_str, id_len)) {
            markers = &hierarchy->subs.detached_markers.vec;
        } else {
            Selva_NodeId node_id;
            struct SelvaHierarchyMetadata *metadata;

            Selva_NodeIdCpy(node_id, id_str);

            metadata = SelvaHierarchy_GetNodeMetadata(hierarchy, node_id);
            if (!metadata) {
                selva_send_error(resp, SELVA_SUBSCRIPTIONS_ENOENT, NULL, 0);
                return;
            }

            markers = &metadata->sub_markers.vec;
        }
    } else {
        selva_send_error(resp, SELVA_SUBSCRIPTIONS_EINVAL, NULL, 0);
        return;
    }

    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;

    selva_send_array(resp, -1);
    SVector_ForeachBegin(&it, markers);
    while ((marker = SVector_Foreach(&it))) {
        SelvaSubscriptions_ReplyWithMarker(resp, marker);
    }
    selva_send_array_end(resp);
}

/*
 * KEY SUB_ID
 */
void SelvaSubscriptions_DelCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    const char *sub_id_str;
    size_t sub_id_len;
    Selva_SubscriptionId sub_id;
    struct Selva_Subscription *sub;
    int argc, err;

    argc = selva_proto_scanf(NULL, buf, len, "%.*s", &sub_id_len, &sub_id_str);
    if (argc != 1) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    err = Selva_SubscriptionStr2id(sub_id, sub_id_str, sub_id_len);
    if (err) {
        selva_send_errorf(resp, err, "Subscription ID");
        return;
    }

    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        selva_send_ll(resp, 0);
        return;
    }

    destroy_sub(hierarchy, sub);

    selva_send_ll(resp, 1);
}

/*
 * KEY SUB_ID MARKER_ID
 */
void SelvaSubscriptions_DelMarkerCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    __auto_finalizer struct finalizer fin;
    const char *sub_id_str;
    size_t sub_id_len;
    Selva_SubscriptionMarkerId marker_id;
    int argc;
    int err;

    finalizer_init(&fin);

    argc = selva_proto_scanf(&fin, buf, len, "%.*s, %" PRImrkId,
                             &sub_id_len, &sub_id_str,
                             &marker_id);
    if (argc != 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = Selva_SubscriptionStr2id(sub_id, sub_id_str, sub_id_len);
    if (err) {
        selva_send_errorf(resp, err, "Subscription ID");
        return;
    }

    struct Selva_Subscription *sub;
    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        selva_send_ll(resp, 0);
        return;
    }

    err = delete_marker(hierarchy, sub, marker_id);
    if (err) {
        selva_send_error(resp, err, NULL, 0);
        return;
    }

    selva_send_ll(resp, 1);
}

static int Subscriptions_OnLoad(void) {
    /*
     * Register commands.
     * All commands are "readonly" because they don't change the
     * observed or serialized key values in any way. This is important
     * because we need to be able to create markers on readonly replicas.
     */
    selva_mk_command(CMD_ID_SUBSCRIPTIONS_ADD, SELVA_CMD_MODE_PURE, "subscriptions.add", SelvaSubscriptions_AddMarkerCommand);
    selva_mk_command(CMD_ID_SUBSCRIPTIONS_ADDALIAS, SELVA_CMD_MODE_PURE, "subscriptions.addAlias", SelvaSubscriptions_AddAliasCommand);
    selva_mk_command(CMD_ID_SUBSCRIPTIONS_ADDMISSING, SELVA_CMD_MODE_PURE, "subscriptions.addMissing", SelvaSubscriptions_AddMissingCommand);
    selva_mk_command(CMD_ID_SUBSCRIPTIONS_ADDTRIGGER, SELVA_CMD_MODE_PURE, "subscriptions.addTrigger", SelvaSubscriptions_AddTriggerCommand);
    selva_mk_command(CMD_ID_SUBSCRIPTIONS_REFRESH, SELVA_CMD_MODE_PURE, "subscriptions.refresh", SelvaSubscriptions_RefreshCommand);
    selva_mk_command(CMD_ID_SUBSCRIPTIONS_LIST, SELVA_CMD_MODE_PURE, "subscriptions.list", SelvaSubscriptions_ListCommand);
    selva_mk_command(CMD_ID_SUBSCRIPTIONS_LISTMISSING, SELVA_CMD_MODE_PURE, "subscriptions.listMissing", SelvaSubscriptions_ListMissingCommand);
    selva_mk_command(CMD_ID_SUBSCRIPTIONS_DEBUG, SELVA_CMD_MODE_PURE, "subscriptions.debug", SelvaSubscriptions_DebugCommand);
    selva_mk_command(CMD_ID_SUBSCRIPTIONS_DEL, SELVA_CMD_MODE_PURE, "subscriptions.del", SelvaSubscriptions_DelCommand);
    selva_mk_command(CMD_ID_SUBSCRIPTIONS_DELMARKER, SELVA_CMD_MODE_PURE, "subscriptions.delMarker", SelvaSubscriptions_DelMarkerCommand);

    return 0;
}
SELVA_ONLOAD(Subscriptions_OnLoad);
