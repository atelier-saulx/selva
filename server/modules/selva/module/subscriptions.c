#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "redismodule.h"
#include "cdefs.h"
#include "selva.h"
#include "async_task.h"
#include "errors.h"
#include "hierarchy.h"
#include "rpn.h"
#include "selva_onload.h"
#include "subscriptions.h"
#include "svector.h"

struct Selva_Subscription {
    Selva_SubscriptionId sub_id;
    RB_ENTRY(Selva_Subscription) _sub_index_entry;
    SVector markers; /* struct Selva_SubscriptionMarker */
};

static void clear_sub(struct SelvaModify_Hierarchy *hierarchy, struct Selva_SubscriptionMarker *marker, Selva_NodeId node_id);

static int marker_svector_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const struct Selva_SubscriptionMarker *a = *(const struct Selva_SubscriptionMarker **)a_raw;
    const struct Selva_SubscriptionMarker *b = *(const struct Selva_SubscriptionMarker **)b_raw;

    const int subdiff = memcmp(a->sub->sub_id, b->sub->sub_id, sizeof(Selva_SubscriptionId));
    if (subdiff) {
        return subdiff;
    }
    return a->marker_id - b->marker_id;
}

static int SelvaSubscription_svector_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const struct Selva_Subscription *a = *(const struct Selva_Subscription **)a_raw;
    const struct Selva_Subscription *b = *(const struct Selva_Subscription **)b_raw;

    return memcmp(a->sub_id, b->sub_id, sizeof(Selva_SubscriptionId));
}

static int subscription_rb_compare(const struct Selva_Subscription *a, const struct Selva_Subscription *b) {
    return memcmp(a->sub_id, b->sub_id, sizeof(Selva_SubscriptionId));
}

/**
 * The given marker flags matches to a hierarchy marker of any kind.
 */
static int isHierarchyMarker(unsigned flags) {
    return !!(flags & SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY);
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
    if ((marker->marker_flags & SELVA_SUBSCRIPTION_FLAG_REF) == SELVA_SUBSCRIPTION_FLAG_REF &&
        !memcmp(node_id, marker->node_id, SELVA_NODE_ID_SIZE)) {
        return 1;
    }

    return 0;
}

RB_PROTOTYPE_STATIC(hierarchy_subscriptions_tree, Selva_Subscription, _sub_index_entry, subscription_rb_compare)
RB_GENERATE_STATIC(hierarchy_subscriptions_tree, Selva_Subscription, _sub_index_entry, subscription_rb_compare)

/*
 * SHA256 to hex string.
 * The destination buffer must be at least SELVA_SUBSCRIPTION_ID_STR_LEN + 1 bytes.
 */
char *Selva_SubscriptionId2str(char dest[static SELVA_SUBSCRIPTION_ID_STR_LEN + 1], const Selva_SubscriptionId sub_id) {
    for (size_t i = 0; i < sizeof(Selva_SubscriptionId); i++) {
        sprintf(dest + 2 * i, "%02x", sub_id[i]);
    }
    dest[SELVA_SUBSCRIPTION_ID_STR_LEN] = '\0';

    return dest;
}

int Selva_SubscriptionStr2id(Selva_SubscriptionId dest, const char *src) {
    char byte[3] = { '\0', '\0', '\0' };

    for (size_t i = 0; i < sizeof(Selva_SubscriptionId); i++) {
        unsigned long v;

        byte[0] = src[2 * i];
        byte[1] = src[2 * i + 1];
        v = strtoul(byte, NULL, 16);

        if (unlikely(v > 0xff)) {
            return SELVA_SUBSCRIPTIONS_EINVAL;
        }

        dest[i] = v;
    }

    return 0;
}

/**
 * Check if field matches to any of the fields specified in the marker.
 */
static int Selva_SubscriptionFieldMatch(const struct Selva_SubscriptionMarker *marker, const char *field) {
    const char *s1 = marker->fields;

    while (*s1 != '\0') {
        const char *s2 = field;

        /* strcmp */
        while (*s1 == *s2++) {
            const char c = *s1++;
            if (c == '\n' || c == '\0') {
                return 1;
            }
        }
        if (*s1 == '\n' && *(s2 - 1) == '\0') {
            return 1;
        }

        /* Skip the rest of the current field */
        while (*s1 != '\0') {
            s1++;
            if (*s1 == '\n') {
                s1++;
                break;
            }
        }
    }

    return 0;
}

static int Selva_SubscriptionFilterMatch(const Selva_NodeId node_id, struct Selva_SubscriptionMarker *marker) {
    struct rpn_ctx *ctx = marker->filter_ctx;
    rpn_token *filter = marker->filter_expression;
    int res = 0;
    int err;

    if (!ctx) {
        return 1;
    }

    rpn_set_reg(ctx, 0, node_id, SELVA_NODE_ID_SIZE, 0);
    err = rpn_bool(ctx, filter, &res);
    if (err) {
        fprintf(stderr, "%s: Expression failed (node: \"%.*s\"): \"%s\"\n",
                __FILE__,
                (int)SELVA_NODE_ID_SIZE, node_id,
                rpn_str_error[err]);
        return 0;
    }

    return res;
}

/*
 * Destroy and free a marker.
 */
static void destroy_marker(struct Selva_SubscriptionMarker *marker) {
    rpn_destroy(marker->filter_ctx);
    RedisModule_Free(marker->filter_expression);
    RedisModule_Free(marker->fields);
    RedisModule_Free(marker);
}

/*
 * Destroy all markers owned by a subscription.
 */
static void destroy_sub(SelvaModify_Hierarchy *hierarchy, struct Selva_Subscription *sub) {
    struct Selva_SubscriptionMarker **it;

    if (SVector_Size(&sub->markers) > 0) {
        svector_autofree SVector markers = {0};

        if (!SVector_Clone(&markers, &sub->markers, NULL)) {
            fprintf(stderr, "%s: Subs ENOMEM, can't destroy a subscription\n", __FILE__);
            return;
        }

        /* TODO implement safe foreach instead of using cloning */
        SVECTOR_FOREACH(it, &markers) {
            struct Selva_SubscriptionMarker *marker = *it;

            if (marker->dir == SELVA_HIERARCHY_TRAVERSAL_NONE) {
                /*
                 * TRAVERSAL_NONE (e.g. root) markers are stored as detached.
                 */
                (void)SVector_Remove(&hierarchy->subs.detached_markers.vec, marker);
            } else {
                /*
                 * Other markers are pointed by one or more nodes in the
                 * hierarchy.
                 */
                clear_sub(hierarchy, marker, marker->node_id);
            }
            destroy_marker(marker);
        }
        /* This isn't technically necessary but we do it for clarity. */
        SVector_Clear(&sub->markers);
    }

    RB_REMOVE(hierarchy_subscriptions_tree, &hierarchy->subs.head, sub);
    SVector_Destroy(&sub->markers);
    RedisModule_Free(sub);
}

/*
 * Destroy all subscription markers.
 */
static void destroy_all_sub_markers(SelvaModify_Hierarchy *hierarchy) {
    struct hierarchy_subscriptions_tree *subs_head = &hierarchy->subs.head;
    struct Selva_Subscription *sub;
    struct Selva_Subscription *next;

	for (sub = RB_MIN(hierarchy_subscriptions_tree, subs_head); sub != NULL; sub = next) {
		next = RB_NEXT(hierarchy_subscriptions_tree, subs_head, sub);
        destroy_sub(hierarchy, sub);
    }
}

/*
 * Destroy all subscriptions and markers in a hierarchy.
 */
void SelvaSubscriptions_DestroyAll(SelvaModify_Hierarchy *hierarchy) {
    SelvaSubscriptions_DestroyDeferredEvents(hierarchy);
    SVector_Destroy(&hierarchy->subs.detached_markers.vec);
    destroy_all_sub_markers(hierarchy);
}

static void init_node_metadata_subs(
        Selva_NodeId id __unused,
        struct SelvaModify_HierarchyMetadata *metadata) {
    /* TODO Lazy alloc would save some memory. */
    SelvaSubscriptions_InitMarkersStruct(&metadata->sub_markers);
}
SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(init_node_metadata_subs);

static void deinit_node_metadata_subs(
        Selva_NodeId id __unused,
        struct SelvaModify_HierarchyMetadata *metadata) {
    SVector_Destroy(&metadata->sub_markers.vec);
}
SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(deinit_node_metadata_subs);

int SelvaSubscriptions_InitMarkersStruct(struct Selva_SubscriptionMarkers *markers) {
    if (!SVector_Init(&markers->vec, 1, marker_svector_compare)) {
        return SELVA_SUBSCRIPTIONS_ENOMEM;
    }

    markers->flags_filter = 0;

    return 0;
}

static struct Selva_Subscription *find_sub(SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id) {
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
    SVector_InsertFast(&sub_markers->vec, marker);
    sub_markers->flags_filter |= marker->marker_flags & SELVA_SUBSCRIPTION_MATCHER_FLAGS_MASK;
}

static void reset_marker_filter(struct Selva_SubscriptionMarkers *sub_markers) {
    struct Selva_SubscriptionMarker **it;

    sub_markers->flags_filter = 0;

    SVECTOR_FOREACH(it, &sub_markers->vec) {
        struct Selva_SubscriptionMarker *marker = *it;

        sub_markers->flags_filter |= marker->marker_flags & SELVA_SUBSCRIPTION_MATCHER_FLAGS_MASK;
    }
}

/*
 * Set a marker to a node metadata.
 */
static int set_node_marker(Selva_NodeId id, void *arg, struct SelvaModify_HierarchyMetadata *metadata) {
    char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
    struct Selva_SubscriptionMarker *marker;

    marker = (struct Selva_SubscriptionMarker *)arg;
    fprintf(stderr, "Set sub %s:%d marker to %.*s\n",
            Selva_SubscriptionId2str(str, marker->sub->sub_id),
            marker->marker_id,
            (int)SELVA_NODE_ID_SIZE, id);
    set_marker(&metadata->sub_markers, marker);

    return 0;
}

static int clear_marker(Selva_NodeId id, void *arg, struct SelvaModify_HierarchyMetadata *metadata) {
    char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
    struct Selva_SubscriptionMarker *marker;

    marker = (struct Selva_SubscriptionMarker*)arg;
    fprintf(stderr, "Clear sub %s from %.*s (nr_subs: %zd)\n",
            Selva_SubscriptionId2str(str, marker->sub->sub_id), (int)SELVA_NODE_ID_SIZE, id,
            SVector_Size(&metadata->sub_markers.vec));
    SVector_Remove(&metadata->sub_markers.vec, marker);
    reset_marker_filter(&metadata->sub_markers);

    return 0;
}

/**
 * Create a subscription.
 */
static struct Selva_Subscription *create_subscription(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_SubscriptionId sub_id) {
    struct Selva_Subscription *sub;

    sub = RedisModule_Calloc(1, sizeof(struct Selva_Subscription));
    if (!sub) {
        return NULL;
    }

    memcpy(sub->sub_id, sub_id, sizeof(sub->sub_id));

    if (!SVector_Init(&sub->markers, 1, marker_svector_compare)) {
        RedisModule_Free(sub);
        return NULL;
    }

    /*
     * Add to the list of subscriptions.
     */
    if (unlikely(RB_INSERT(hierarchy_subscriptions_tree, &hierarchy->subs.head, sub) != NULL)) {
        SVector_Destroy(&sub->markers);
        RedisModule_Free(sub);
        return NULL;
    }

    return sub;
}

/**
 * Add a new marker to a subscription.
 * Specifiers:
 *  - n = node_id, copied.
 *  - d = traversal direction, value.
 *  - r = RPN context and expression, pointers used directly and must be valid
 *        until the subscription is deleted.
 *  - f = field names, separated by `\n` and terminated by `\0`. The string is
 *        duplicated.
 */
static int Selva_AddSubscriptionMarker(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id,
        unsigned flags,
        const char *fmt,
        ...) {
    va_list args;
    struct Selva_Subscription *sub;
    struct Selva_SubscriptionMarker *marker;

    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        sub = create_subscription(hierarchy, sub_id);
        if (!sub) {
            return SELVA_SUBSCRIPTIONS_ENOMEM;
        }
    }

    if (find_sub_marker(sub, marker_id)) {
        return SELVA_SUBSCRIPTIONS_EEXIST;
    }

    marker = RedisModule_Calloc(1, sizeof(struct Selva_SubscriptionMarker));
    if (!marker) {
        /* The subscription won't be freed. */
        return SELVA_SUBSCRIPTIONS_ENOMEM;
    }

    marker->marker_id = marker_id;
    marker->marker_flags = flags;
    marker->sub = sub;

    va_start(args, fmt);
    while (*fmt != '\0') {
        char c = *fmt;
        switch (c) {
        case 'n': /* node_id */
            memcpy(marker->node_id, va_arg(args, char *), SELVA_NODE_ID_SIZE);
            break;
        case 'd': /* Hierarchy traversal direction */
            marker->dir = va_arg(args, enum SelvaModify_HierarchyTraversal);
            break;
        case 'r': /* RPN filter */
            marker->filter_ctx = va_arg(args, struct rpn_ctx *);
            marker->filter_expression = va_arg(args, rpn_token *);
            break;
        case 'f': /* Fields */
            marker->fields = RedisModule_Strdup(va_arg(args, char *));
            break;
        default:
            {
                char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

                fprintf(stderr, "%s: Invalid marker specifier '%c' for subscription %s\n",
                        __FILE__,
                        c, Selva_SubscriptionId2str(str, sub_id));
                return SELVA_SUBSCRIPTIONS_EINVAL;
            }
        }
        fmt++;
    }
    va_end(args);

    /* We already checked that the id doesn't exist. */
    (void)SVector_InsertFast(&sub->markers, marker);

    return 0;
}

struct Selva_SubscriptionMarker *SelvaSubscriptions_GetMarker(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id) {
    struct Selva_Subscription *sub;

    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        return NULL;
    }

    return find_sub_marker(sub, marker_id);
}

/**
 * Set an opaque subscription marker on the node metadata.
 * This function can be used in traversal to refresh markers on the fly and thus
 * avoiding calling a separate refresh on the subscription marker.
 */
void Selva_Subscriptions_SetMarker(
        struct SelvaModify_HierarchyMetadata *metadata,
        struct Selva_SubscriptionMarker *marker) {
    struct Selva_SubscriptionMarkers *sub_markers = &metadata->sub_markers;

    set_marker(sub_markers, marker);
}


static int refreshSubscription(struct SelvaModify_Hierarchy *hierarchy, struct Selva_Subscription *sub) {
    struct Selva_SubscriptionMarker **it;
    int res = 0;

    SVECTOR_FOREACH(it, &sub->markers) {
        struct Selva_SubscriptionMarker *marker = *it;
        int err;

        if (marker->dir == SELVA_HIERARCHY_TRAVERSAL_NONE ||
            (marker->marker_flags & SELVA_SUBSCRIPTION_FLAG_DETACH)) {
            /*
             * This is a non-traversing marker but it needs to exist in the
             * detached markers.
             */
            set_marker(&hierarchy->subs.detached_markers, marker);
            continue;
        }

        /*
         * Set subscription markers.
         */
        struct SelvaModify_HierarchyCallback cb = {
            .node_cb = set_node_marker,
            .node_arg = marker,
        };

        err = SelvaModify_TraverseHierarchy(hierarchy, marker->node_id, marker->dir, &cb);
        if (err) {
            char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

            fprintf(stderr, "%s: Could not fully apply a subscription: %s\n",
                    __FILE__, Selva_SubscriptionId2str(str, sub->sub_id));
            res = err; /* Report the last error */
        }
    }

    return res;
}

int SelvaSubscriptions_Refresh(struct SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id) {
    struct Selva_Subscription *sub;

    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        return SELVA_SUBSCRIPTIONS_ENOENT;
    }

    return refreshSubscription(hierarchy, sub);
}

void SelvaSubscriptions_RefreshByMarker(struct SelvaModify_Hierarchy *hierarchy, SVector *markers) {
    struct Selva_SubscriptionMarker **it;

    SVECTOR_FOREACH(it, markers) {
        struct Selva_SubscriptionMarker *marker = *it;

        /* Ignore errors for now. */
        (void)refreshSubscription(hierarchy, marker->sub);
    }
}

/**
 * Clear subscription starting from node_id.
 * Clear subscription starting from node_id and remove the subscription if
 * node_id is the starting point.
 */
static void clear_sub(struct SelvaModify_Hierarchy *hierarchy, struct Selva_SubscriptionMarker *marker, Selva_NodeId node_id) {
    struct SelvaModify_HierarchyCallback cb = {
        .node_cb = clear_marker,
        .node_arg = marker,
    };

    /*
     * Remove subscription markers.
     */
    (void)SelvaModify_TraverseHierarchy(hierarchy, node_id, marker->dir, &cb);
}

void SelvaSubscriptions_Delete(struct SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id) {
    struct Selva_Subscription *sub;

    sub = find_sub(hierarchy, sub_id);
    if (sub) {
        destroy_sub(hierarchy, sub);
    }
}

/*
 * Clear all markers from a node.
 */
void SelvaSubscriptions_ClearAllMarkers(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetadata *metadata) {
    const size_t nr_markers = SVector_Size(&metadata->sub_markers.vec);
    svector_autofree SVector markers = {0};
    struct Selva_SubscriptionMarker **it;

    if (nr_markers == 0) {
        return;
    }

    fprintf(stderr, "Removing %zu subscription markers from %.*s\n",
            nr_markers, (int)SELVA_NODE_ID_SIZE, node_id);

    if (unlikely(!SVector_Clone(&markers, &metadata->sub_markers.vec, NULL))) {
        fprintf(stderr, "%s: %s ENOMEM\n", __FILE__, __func__);
        return;
    }

    /*
     * Remove each subscription marker from this node and its ancestors/descendants.
     */
    SVECTOR_FOREACH(it, &markers) {
        struct Selva_SubscriptionMarker *marker = *it;

        clear_sub(hierarchy, marker, node_id);
    }
    SVector_Clear(&metadata->sub_markers.vec);
}

int SelvaSubscriptions_InitDeferredEvents(struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    if (unlikely(!SVector_Init(&def->subs, 2, SelvaSubscription_svector_compare))) {
        return SELVA_SUBSCRIPTIONS_ENOMEM;
    }

    return 0;
}

void SelvaSubscriptions_DestroyDeferredEvents(struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    if (!def) {
        return;
    }

    SVector_Destroy(&def->subs);
}

static void defer_hierarchy_events(struct SelvaModify_Hierarchy *hierarchy,
                                   const Selva_NodeId node_id __unused,
                                   const struct Selva_SubscriptionMarkers *sub_markers) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    struct Selva_SubscriptionMarker **it;

    if (isHierarchyMarker(sub_markers->flags_filter)) {
        SVECTOR_FOREACH(it, &sub_markers->vec) {
            struct Selva_SubscriptionMarker *marker = *it;

            /*
             * We cannot call inhibitMarkerEvent() here because the client needs
             * to refresh the subscription even if this node is not part of the
             * marker filter.
             * E.g. consider subscription marker for children of a node. In this
             * case we need to apply the marker to any new children.
             */
            if (isHierarchyMarker(marker->marker_flags)) {
                SVector_InsertFast(&def->subs, marker->sub);
            }
        }
    }
}

void SelvaSubscriptions_DeferHierarchyEvents(struct SelvaModify_Hierarchy *hierarchy,
                                             const Selva_NodeId node_id,
                                             const struct SelvaModify_HierarchyMetadata *metadata) {
    /* Detached markers. */
    defer_hierarchy_events(hierarchy, node_id, &hierarchy->subs.detached_markers);

    /* Markers on the node.  */
    defer_hierarchy_events(hierarchy, node_id, &metadata->sub_markers);
}

static void field_change_precheck(
        const Selva_NodeId node_id,
        const struct Selva_SubscriptionMarkers *sub_markers) {
    const unsigned flags = SELVA_SUBSCRIPTION_FLAG_CH_FIELD;

    if ((sub_markers->flags_filter & flags) == flags) {
        struct Selva_SubscriptionMarker **it;

        SVECTOR_FOREACH(it, &sub_markers->vec) {
            struct Selva_SubscriptionMarker *marker = *it;

            if (((marker->marker_flags & flags) == flags)) {
                /*
                 * Store the filter result before any changes to the node.
                 * We assume that SelvaSubscriptions_DeferFieldChangeEvents()
                 * is called before this function is called for another node.
                 */
                memcpy(marker->filter_history.node_id, node_id, SELVA_NODE_ID_SIZE);
                marker->filter_history.res = Selva_SubscriptionFilterMatch(node_id, marker);
            }
        }
    }
}

void SelvaSubscriptions_FieldChangePrecheck(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata) {
    /* Detached markers. */
    field_change_precheck(node_id, &hierarchy->subs.detached_markers);

    if (!metadata) {
        fprintf(stderr, "%s:%d Node metadata missing\n", __FILE__, __LINE__);
        return;
    }

    /* Markers on the node. */
    field_change_precheck(node_id, &metadata->sub_markers);
}

static void defer_field_change_events(struct SelvaModify_Hierarchy *hierarchy,
                                      const Selva_NodeId node_id,
                                      const struct Selva_SubscriptionMarkers *sub_markers,
                                      const char *field) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    const unsigned flags = SELVA_SUBSCRIPTION_FLAG_CH_FIELD;

    if ((sub_markers->flags_filter & flags) == flags) {
        struct Selva_SubscriptionMarker **it;

        SVECTOR_FOREACH(it, &sub_markers->vec) {
            struct Selva_SubscriptionMarker *marker = *it;

            if (((marker->marker_flags & flags) == flags) &&
                !inhibitMarkerEvent(node_id, marker) &&
                Selva_SubscriptionFieldMatch(marker, field)) {
                const int after = Selva_SubscriptionFilterMatch(node_id, marker);

                /*
                 * An event should be sent if the filter resolves to true now
                 * or if its outcome was changed from true to false.
                 *
                 * after || (before && !after)
                 *     0  0  0       0 1    0
                 *     0  1  1       1 1    0
                 *     1  1  0       0 0    1
                 *     1  1  1       0 0    1
                 *
                 * which is equivalent to
                 *
                 * after || before
                 */
                if (after ||
                    /* before */
                    (marker->filter_history.res && memcmp(marker->filter_history.node_id, node_id, SELVA_NODE_ID_SIZE))) {
                    SVector_InsertFast(&def->subs, marker->sub);
                }
            }
        }
    }
}

void SelvaSubscriptions_DeferFieldChangeEvents(struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata,
        const char *field) {
    /* Detached markers. */
    defer_field_change_events(hierarchy, node_id, &hierarchy->subs.detached_markers, field);

    if (!metadata) {
        fprintf(stderr, "%s:%d Node metadata missing\n", __FILE__, __LINE__);
        return;
    }

    /* Markers on the node. */
    defer_field_change_events(hierarchy, node_id, &metadata->sub_markers, field);
}

void SelvaSubscriptions_SendDeferredEvents(struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    struct Selva_Subscription **it;

    SVECTOR_FOREACH(it, &def->subs) {
        struct Selva_Subscription *sub = *it;
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

        fprintf(stderr, "%s: publish %s\n", __FILE__, Selva_SubscriptionId2str(str, sub->sub_id));
        SelvaModify_PublishSubscriptionUpdate(sub->sub_id);
    }
    SVector_Clear(&def->subs);
}

static int parse_subscription_id(Selva_SubscriptionId id, RedisModuleString *arg) {
    TO_STR(arg);

    if (arg_len != SELVA_SUBSCRIPTION_ID_STR_LEN) {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    return Selva_SubscriptionStr2id(id, arg_str);
}

static int parse_marker_id(Selva_SubscriptionMarkerId *marker_id, RedisModuleString *arg) {
    long long ll;

    if (RedisModule_StringToLongLong(arg, &ll) != REDISMODULE_OK) {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    *marker_id = (Selva_SubscriptionMarkerId)ll;
    return 0;
}

static int parse_subscription_type(enum SelvaModify_HierarchyTraversal *dir, RedisModuleString *arg) {
    TO_STR(arg);

    if (!strcmp("none", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_NONE;
    } else if (!strcmp("node", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_NODE;
    } else if (!strcmp("children", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_CHILDREN;
    } else if (!strcmp("parents", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_PARENTS;
    } else if (!strcmp("ancestors", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS;
    } else if (!strcmp("descendants", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS;
    } else {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    return 0;
}

/*
 * KEY SUB_ID MARKER_ID node|ancestors|descendants NODE_ID [fields <fieldnames \n separated>] [filter expression] [filter args...]
 */
int Selva_SubscribeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const size_t ARGV_REDIS_KEY     = 1;
    const size_t ARGV_SUB_ID        = 2;
    const size_t ARGV_MARKER_ID     = 3;
    const size_t ARGV_MARKER_TYPE   = 4;
    const size_t ARGV_NODE_ID       = 5;
    const size_t ARGV_FIELDS        = 6;
    const size_t ARGV_FIELD_NAMES   = 7;
    size_t ARGV_FILTER_EXPR         = 6;
    size_t ARGV_FILTER_ARGS         = 7;
#define SHIFT_ARGS(i) \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    if (argc < (int)(ARGV_NODE_ID + 1)) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = parse_subscription_id(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    /*
     * Get the marker id.
     */
    Selva_SubscriptionMarkerId marker_id;
    err = parse_marker_id(&marker_id, argv[ARGV_MARKER_ID]);
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    if (SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id)) {
        /* Marker already created. */
        return RedisModule_ReplyWithLongLong(ctx, 1);
    }

    /*
     * Parse the traversal argument.
     */
    enum SelvaModify_HierarchyTraversal sub_dir;
    err = parse_subscription_type(&sub_dir, argv[ARGV_MARKER_TYPE]);
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    /*
     * Get the nodeId.
     */
    Selva_NodeId node_id;
    size_t len;
    const char *str = RedisModule_StringPtrLen(argv[ARGV_NODE_ID], &len);
    memset(node_id, 0, SELVA_NODE_ID_SIZE);
    memcpy(node_id, str, min(SELVA_NODE_ID_SIZE, len));

    /*
     * Get field names for change events.
     * Optional.
     */
    const char *fields = NULL;
    if (argc > (int)ARGV_FIELD_NAMES) {
        const char *fields_arg;

        fields_arg = RedisModule_StringPtrLen(argv[ARGV_FIELDS], NULL);
        if(strcmp(fields_arg, "fields")) {
            return replyWithSelvaError(ctx, SELVA_SUBSCRIPTIONS_EINVAL);
        }

        fields = RedisModule_StringPtrLen(argv[ARGV_FIELD_NAMES], NULL);

        SHIFT_ARGS(2);
    }

    /*
     * Parse & compile the filter expression.
     * Optional.
     */
    struct rpn_ctx *filter_ctx = NULL;
    rpn_token *filter_expression = NULL;
    if (argc > (int)ARGV_FILTER_EXPR) {
        if (argc >= (int)ARGV_FILTER_EXPR + 1) {
            const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
            const char *input;
            size_t input_len;

            filter_ctx = rpn_init(ctx, nr_reg);
            if (!filter_ctx) {
                err = SELVA_SUBSCRIPTIONS_ENOMEM;
                goto out;
            }

            /*
             * Compile the filter expression.
             */
            input = RedisModule_StringPtrLen(argv[ARGV_FILTER_EXPR], &input_len);
            filter_expression = rpn_compile(input, input_len);
            if (!filter_expression) {
                fprintf(stderr, "%s: Failed to compile a filter expression: %.*s\n",
                        __FILE__,
                        (int)input_len, input);
                err = SELVA_RPN_ECOMP;
                goto out;
            }

            /*
             * Get the filter expression arguments and set them to the registers.
             */
            for (size_t i = ARGV_FILTER_ARGS; i < (size_t)argc; i++) {
                /* reg[0] is reserved for the current nodeId */
                const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
                size_t str_len;
                const char *str;
                char *arg;

                /*
                 * Args needs to be duplicated so the strings don't get freed
                 * when the command returns.
                 */
                str = RedisModule_StringPtrLen(argv[i], &str_len);
                str_len++;
                arg = RedisModule_Alloc(str_len);
                memcpy(arg, str, str_len);

                rpn_set_reg(filter_ctx, reg_i, arg, str_len, RPN_SET_REG_FLAG_RMFREE);
            }
        }
    }

    unsigned marker_flags = 0;

    if (!memcmp(node_id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE)) {
        /*
         * A root node marker is a special case which is stored as detached to
         * save time and space.
         */
        marker_flags = SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY | SELVA_SUBSCRIPTION_FLAG_DETACH;
    } else if (sub_dir == SELVA_HIERARCHY_TRAVERSAL_CHILDREN || sub_dir == SELVA_HIERARCHY_TRAVERSAL_PARENTS) {
        /*
         * RFE We might want to have an arg for REF flag
         * but currently it seems to be enough to support
         * it only for these specific traversal types.
         */
        marker_flags = SELVA_SUBSCRIPTION_FLAG_REF | SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY;
    } else if (sub_dir != SELVA_HIERARCHY_TRAVERSAL_NONE) {
        marker_flags = SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY;
    }

    if (fields) {
        marker_flags |= SELVA_SUBSCRIPTION_FLAG_CH_FIELD;
        err = Selva_AddSubscriptionMarker(hierarchy, sub_id, marker_id, marker_flags, "ndfr",
                                          node_id, sub_dir, fields,
                                          filter_ctx, filter_expression);
    } else {
        err = Selva_AddSubscriptionMarker(hierarchy, sub_id, marker_id, marker_flags, "ndr",
                                          node_id, sub_dir,
                                          filter_ctx, filter_expression);
    }
    if (err == 0 || err == SELVA_SUBSCRIPTIONS_EEXIST) {
        if (err == SELVA_SUBSCRIPTIONS_EEXIST) {
            /* This shouldn't happen as we check for this already before. */
            rpn_destroy(filter_ctx);
            RedisModule_Free(filter_expression);

            return RedisModule_ReplyWithLongLong(ctx, 1);
        }

        RedisModule_ReplyWithLongLong(ctx, 1);
#if 0
        RedisModule_ReplicateVerbatim(ctx);
#endif
    } else if (err) {
out:
        if (filter_ctx) {
            rpn_destroy(filter_ctx);
            RedisModule_Free(filter_expression);
        }
        replyWithSelvaError(ctx, err);
    }

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

/*
 * SELVA.SUBSCRIPTIONS.addMarkerField KEY SUB_ID MARKER_ID field_names
 */
int Selva_AddSubscriptionMarkerFieldsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    int err;

    if (argc != 5) {
        return RedisModule_WrongArity(ctx);
    }

    const size_t ARGV_REDIS_KEY     = 1;
    const size_t ARGV_SUB_ID        = 2;
    const size_t ARGV_MARKER_ID     = 3;
    const size_t ARGV_FIELD_NAMES   = 4;

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = parse_subscription_id(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    /*
     * Get the marker id.
     */
    Selva_SubscriptionMarkerId marker_id;
    err = parse_marker_id(&marker_id, argv[ARGV_MARKER_ID]);
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    /*
     * Get field names for change events.
     */
    const char *new_fields = NULL;
    size_t new_fields_len;
    new_fields = RedisModule_StringPtrLen(argv[ARGV_FIELD_NAMES], &new_fields_len);

    struct Selva_SubscriptionMarker *marker;
    marker = SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id);
    if (!marker) {
        return replyWithSelvaError(ctx, SELVA_SUBSCRIPTIONS_ENOENT);
    }

    char *fields = marker->fields;
    const size_t old_len = fields ? strlen(fields) : 0;

    fields = RedisModule_Realloc(fields, old_len + new_fields_len + 2);
    if (!fields) {
        return replyWithSelvaError(ctx, SELVA_SUBSCRIPTIONS_ENOMEM);
    }

    if (old_len > 0) {
        fields[old_len] = '\n';
        memcpy(fields + old_len + 1, new_fields, new_fields_len + 1);
    } else {
        memcpy(fields, new_fields, new_fields_len + 1);
    }

    marker->fields = fields;
    marker->marker_flags |= SELVA_SUBSCRIPTION_FLAG_CH_FIELD; /* This is not set yet if old was NULL. */

    RedisModule_ReplyWithLongLong(ctx, 1);
    return REDISMODULE_OK;
}

/*
 * SELVA.SUBSCRIPTIONS.refresh KEY SUB_ID
 */
int Selva_RefreshSubscriptionCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    int err;

    if (argc != 3) {
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
        fprintf(stderr, "%s: Invalid sub_id\n", __FILE__);
        return replyWithSelvaError(ctx, err);
    }

    struct Selva_Subscription *sub;
    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        replyWithSelvaError(ctx, SELVA_SUBSCRIPTIONS_ENOENT);
        return REDISMODULE_OK;
    }

    err = refreshSubscription(hierarchy, sub);
    if (err) {
        replyWithSelvaError(ctx, err);
    } else {
        RedisModule_ReplyWithLongLong(ctx, 1);
    }

    return REDISMODULE_OK;
}

/*
 * KEY
 */
int Selva_SubscriptionsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    const size_t ARGV_REDIS_KEY = 1;

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    struct Selva_Subscription *sub;
    size_t array_len = 0;

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    RB_FOREACH(sub, hierarchy_subscriptions_tree, &hierarchy->subs.head) {
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

        RedisModule_ReplyWithStringBuffer(ctx, Selva_SubscriptionId2str(str, sub->sub_id), SELVA_SUBSCRIPTION_ID_STR_LEN);
        array_len++;
    }

    RedisModule_ReplySetArrayLength(ctx, array_len);

    return REDISMODULE_OK;
}

/*
 * KEY SUB_ID
 */
int Selva_SubscriptionDebugCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    int err;

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    const size_t ARGV_REDIS_KEY = 1;
    const size_t ARGV_SUB_ID    = 2;

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    Selva_SubscriptionId sub_id;
    err = parse_subscription_id(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        fprintf(stderr, "%s: Invalid sub_id\n", __FILE__);
        return replyWithSelvaError(ctx, err);
    }

    struct Selva_Subscription *sub;
    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        replyWithSelvaError(ctx, SELVA_SUBSCRIPTIONS_ENOENT);
        return REDISMODULE_OK;
    }

    size_t array_len = 0;
    struct Selva_SubscriptionMarker **it;
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    SVECTOR_FOREACH(it, &sub->markers) {
        RedisModuleString *str;
        struct Selva_SubscriptionMarker *marker = *it;

        str = RedisModule_CreateStringPrintf(ctx,
                "marker_id: %d\n"
                "flags: %u\n"
                "node_id: \"%.*s\"\n"
                "dir: %s\n"
                "expression: %s\n"
                "fields: \"%s\"",
                (int)marker->marker_id,
                marker->marker_flags,
                (int)SELVA_NODE_ID_SIZE, marker->node_id,
                SelvaModify_HierarchyDir2str(marker->dir),
                (marker->filter_ctx) ? "set" : "unset",
                marker->fields);
        if (!str) {
            return REDISMODULE_ERR;
        }
        RedisModule_ReplyWithString(ctx, str);
        array_len++;
    }
    RedisModule_ReplySetArrayLength(ctx, array_len);

    return REDISMODULE_OK;
}

/*
 * KEY SUB_ID
 */
int Selva_UnsubscribeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    int err;

    if (argc != 3) {
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
        fprintf(stderr, "%s: Invalid sub_id\n", __FILE__);
        return replyWithSelvaError(ctx, err);
    }

    struct Selva_Subscription *sub;
    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        RedisModule_ReplyWithLongLong(ctx, 0);
        return REDISMODULE_OK;
    }

    destroy_sub(hierarchy, sub);

    RedisModule_ReplyWithLongLong(ctx, 1);
    return REDISMODULE_OK;
}

static int Hierarchy_Subscriptions_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.subscriptions.add", Selva_SubscribeCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.addMarkerFields", Selva_AddSubscriptionMarkerFieldsCommand, "write deny-oom", 1, 1 ,1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.refresh", Selva_RefreshSubscriptionCommand, "write deny-oom", 1, 1 ,1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.list", Selva_SubscriptionsCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.debug", Selva_SubscriptionDebugCommand, "readonly deny-script", 1, 1, 1) ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.del", Selva_UnsubscribeCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Hierarchy_Subscriptions_OnLoad);
