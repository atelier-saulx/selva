#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "redismodule.h"
#include "cdefs.h"
#include "cstrings.h"
#include "selva.h"
#include "svector.h"
#include "arg_parser.h"
#include "async_task.h"
#include "errors.h"
#include "hierarchy.h"
#include "resolve.h"
#include "rpn.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "subscriptions.h"

struct Selva_Subscription {
    Selva_SubscriptionId sub_id;
    RB_ENTRY(Selva_Subscription) _sub_index_entry;
    SVector markers; /* struct Selva_SubscriptionMarker */
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

RB_PROTOTYPE_STATIC(hierarchy_subscriptions_tree, Selva_Subscription, _sub_index_entry, subscription_rb_compare)
RB_GENERATE_STATIC(hierarchy_subscriptions_tree, Selva_Subscription, _sub_index_entry, subscription_rb_compare)

/**
 * The given marker flags matches to a hierarchy marker of any kind.
 */
static int isHierarchyMarker(unsigned short flags) {
    return !!(flags & SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY);
}

static int isAliasMarker(unsigned short flags) {
    return !!(flags & SELVA_SUBSCRIPTION_FLAG_ALIAS);
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


/*
 * SHA256 to hex string.
 * The destination buffer must be at least SELVA_SUBSCRIPTION_ID_STR_LEN + 1 bytes.
 */
char *Selva_SubscriptionId2str(char dest[static SELVA_SUBSCRIPTION_ID_STR_LEN + 1], const Selva_SubscriptionId sub_id) {
    for (size_t i = 0; i < sizeof(Selva_SubscriptionId); i++) {
        snprintf(dest + 2 * i, SELVA_SUBSCRIPTION_ID_STR_LEN + 1, "%02x", sub_id[i]);
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
    int match = 0;

    if (!!(marker->marker_flags & SELVA_SUBSCRIPTION_FLAG_CH_FIELD) && marker->fields) {
        const char *list = marker->fields;

        if (list[0] == '\0') {
            /* Empty string equals to a wildcard */
            match = 1;
        } else {
            /* Test if field matches to any of the fields in list. */
            const char *sep = ".";
            char *p;

            match = stringlist_searchn(list, field, strlen(field));

            /* Test for each subfield if there was no exact match. */
            if (!match && (p = strstr(field, sep))) {
                do {
                    const size_t len = (ptrdiff_t)p++ - (ptrdiff_t)field;

                    match = stringlist_searchn(list, field, len);
                } while (!match && p && (p = strstr(p, sep)));
            }
        }
    }

    return match;
}

/**
 * Match subscription marker with the RPN expression filter.
 */
static int Selva_SubscriptionFilterMatch(RedisModuleCtx *ctx, const Selva_NodeId node_id, struct Selva_SubscriptionMarker *marker) {
    struct rpn_ctx *filter_ctx = marker->filter_ctx;
    int res = 0;
    int err;

    /* When no filter is set the result should be true. */
    if (!filter_ctx) {
        return 1;
    }

    rpn_set_reg(filter_ctx, 0, node_id, SELVA_NODE_ID_SIZE, 0);
    err = rpn_bool(ctx, filter_ctx, marker->filter_expression, &res);
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
#if MEM_DEBUG
    if (marker->filter_expression) {
        memset(marker->filter_expression, 0, sizeof(*marker->filter_expression));
    }
    if (marker->fields) {
        memset(marker->fields, 0, sizeof(*marker->fields));
    }
    if (marker) {
        memset(marker, 0, sizeof(*marker));
    }
#endif
    RedisModule_Free(marker->filter_expression);
    RedisModule_Free(marker->fields);
    RedisModule_Free(marker);
}

static void remove_sub_missing_accessor_markers(SelvaModify_Hierarchy *hierarchy, struct Selva_Subscription *sub) {
    struct SelvaObject *missing = hierarchy->subs.missing;
    SelvaObject_Iterator *it_missing;
    struct SelvaObject *subs;
    const char *id;
    char sub_id_str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
    RedisModuleString *sub_id;

    if (!sub || !missing) {
        return;
    }

    sub_id = RedisModule_CreateString(NULL, Selva_SubscriptionId2str(sub_id_str, sub->sub_id), SELVA_SUBSCRIPTION_ID_STR_LEN);
    if (!sub_id) {
        fprintf(stderr, "%s: Out of memory while removing missing markers", __FILE__);
        return;
    }

    it_missing = SelvaObject_ForeachBegin(missing);
    while ((subs = (struct SelvaObject *)SelvaObject_ForeachValue(missing, &it_missing, &id, SELVA_OBJECT_OBJECT))) {
        SelvaObject_Iterator *it_subs;
        struct Selva_Subscription *sub_p;

        /* Delete all keys of this subscription stored under id. */
        it_subs = SelvaObject_ForeachBegin(subs);
        while ((sub_p = (struct Selva_Subscription *)SelvaObject_ForeachValue(subs, &it_subs, NULL, SELVA_OBJECT_POINTER))) {
            SelvaObject_DelKey(subs, sub_id);
        }

        /* Delete the id key if the object is now empty. */
        if (SelvaObject_Len(subs, NULL) == 0) {
            SelvaObject_DelKeyStr(missing, id, strlen(id));
        }
    }

    RedisModule_FreeString(NULL, sub_id);
}

static void remove_sub_markers(SelvaModify_Hierarchy *hierarchy, struct Selva_Subscription *sub) {
    if (SVector_Size(&sub->markers) > 0) {
        struct Selva_SubscriptionMarker *marker;

        while ((marker = SVector_Shift(&sub->markers))) {
            if (marker->dir == SELVA_HIERARCHY_TRAVERSAL_NONE ||
                (marker->marker_flags & (SELVA_SUBSCRIPTION_FLAG_DETACH | SELVA_SUBSCRIPTION_FLAG_TRIGGER))) {
                (void)SVector_Remove(&hierarchy->subs.detached_markers.vec, marker);
            } else {
                /*
                 * Other markers are normally pointed by one or more nodes in
                 * the hierarchy.
                 */
                clear_sub(hierarchy, marker, marker->node_id);
            }
            destroy_marker(marker);
        }
        SVector_ShiftReset(&sub->markers);
    }
}

/*
 * Destroy all markers owned by a subscription.
 */
static void destroy_sub(SelvaModify_Hierarchy *hierarchy, struct Selva_Subscription *sub) {
    /* Destroy markers. */
    remove_sub_markers(hierarchy, sub);

    /* Remove missing accessor markers. */
    remove_sub_missing_accessor_markers(hierarchy, sub);

    RB_REMOVE(hierarchy_subscriptions_tree, &hierarchy->subs.head, sub);
    SVector_Destroy(&sub->markers);
#if MEM_DEBUG
    memset(sub, 0, sizeof(*sub));
#endif
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
    destroy_all_sub_markers(hierarchy);
    SelvaObject_Destroy(hierarchy->subs.missing);

    /*
     * Do this as the last step because destroy_all_sub_markers() will access
     * the vector.
     */
    SVector_Destroy(&hierarchy->subs.detached_markers.vec);
}

static void init_node_metadata_subs(
        Selva_NodeId id __unused,
        struct SelvaModify_HierarchyMetadata *metadata) {
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
    if (!SVector_Init(&markers->vec, 0, marker_svector_compare)) {
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
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;

    sub_markers->flags_filter = 0;

    SVector_ForeachBegin(&it, &sub_markers->vec);
    while ((marker = SVector_Foreach(&it))) {
        sub_markers->flags_filter |= marker->marker_flags & SELVA_SUBSCRIPTION_MATCHER_FLAGS_MASK;
    }
}

static void clear_marker(struct Selva_SubscriptionMarkers *sub_markers, struct Selva_SubscriptionMarker *marker) {
    SVector_Remove(&sub_markers->vec, marker);
    reset_marker_filter(sub_markers);
}

/*
 * Set a marker to a node metadata.
 */
static int set_node_marker_cb(Selva_NodeId id __unused, void *arg, struct SelvaModify_HierarchyMetadata *metadata) {
    struct Selva_SubscriptionMarker *marker = (struct Selva_SubscriptionMarker *)arg;
#if 0
    char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

    fprintf(stderr, "Set sub %s:%d marker to %.*s\n",
            Selva_SubscriptionId2str(str, marker->sub->sub_id),
            marker->marker_id,
            (int)SELVA_NODE_ID_SIZE, id);
#endif
    set_marker(&metadata->sub_markers, marker);

    return 0;
}

static int clear_node_marker_cb(Selva_NodeId id __unused, void *arg, struct SelvaModify_HierarchyMetadata *metadata) {
    struct Selva_SubscriptionMarker *marker = (struct Selva_SubscriptionMarker*)arg;
#if 0
    char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

    fprintf(stderr, "Clear sub %s from %.*s (nr_subs: %zd)\n",
            Selva_SubscriptionId2str(str, marker->sub->sub_id), (int)SELVA_NODE_ID_SIZE, id,
            SVector_Size(&metadata->sub_markers.vec));
#endif
    clear_marker(&metadata->sub_markers, marker);

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
 *  - E = Event type. Cannot coexist with n.
 *  - e = RPN context and expression, pointers used directly and must be valid
 *        until the subscription is deleted.
 *  - f = field names, separated by `\n` and terminated by `\0`. The string is
 *        duplicated.
 *  - r = ref_field for SELVA_HIERARCHY_TRAVERSAL_REF.
 */
static int Selva_AddSubscriptionMarker(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id,
        unsigned short flags,
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
        case 'E': /* Trigger event_type */
            marker->event_type = va_arg(args, enum Selva_SubscriptionTriggerType);
            break;
        case 'd': /* Hierarchy traversal direction */
            marker->dir = va_arg(args, enum SelvaModify_HierarchyTraversal);
            break;
        case 'e': /* RPN expression filter */
            marker->filter_ctx = va_arg(args, struct rpn_ctx *);
            marker->filter_expression = va_arg(args, rpn_token *);
            break;
        case 'f': /* Fields */
            marker->fields = RedisModule_Strdup(va_arg(args, char *));
            break;
        case 'n': /* node_id */
            memcpy(marker->node_id, va_arg(args, char *), SELVA_NODE_ID_SIZE);
            break;
        case 'r': /* ref_field */
            {
                const char *ref_field = va_arg(args, char *);

                marker->ref_field = !ref_field ? NULL : RedisModule_Strdup(ref_field);
            }
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

int Selva_AddSubscriptionAliasMarker(
        SelvaModify_Hierarchy *hierarchy,
        Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id,
        RedisModuleString *alias_name,
        Selva_NodeId node_id
    ) {
    struct rpn_ctx *filter_ctx;
    rpn_token *filter_expression;
    int err = 0;

    if (SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id)) {
        /* Marker already created. */
        return SELVA_SUBSCRIPTIONS_EEXIST;
    }

    filter_ctx = rpn_init(3);
    if (!filter_ctx) {
        err = SELVA_SUBSCRIPTIONS_ENOMEM;
        goto out;
    }

    /*
     * Compile the filter.
     * `ALIAS_NAME in aliases`
     */
    filter_expression = rpn_compile("$1 $2 a", 7);
    if (!filter_expression) {
        fprintf(stderr, "%s: Failed to compile a filter for alias \"%s\"\n",
                __FILE__, RedisModule_StringPtrLen(alias_name, NULL));
        err = SELVA_RPN_ECOMP;
        goto out;
    }

    /* Set RPN registers */
    /* TODO Handle errors */
    (void)rpn_set_reg_rm(filter_ctx, 1, alias_name);
    (void)rpn_set_reg(filter_ctx, 2, SELVA_ALIASES_FIELD, sizeof(SELVA_ALIASES_FIELD), 0);

    const unsigned short marker_flags = SELVA_SUBSCRIPTION_FLAG_ALIAS;
    const enum SelvaModify_HierarchyTraversal sub_dir = SELVA_HIERARCHY_TRAVERSAL_NODE;
    err = Selva_AddSubscriptionMarker(hierarchy, sub_id, marker_id, marker_flags, "nde",
                                      node_id, sub_dir,
                                      filter_ctx, filter_expression);
out:
    if (err && filter_ctx) {
        rpn_destroy(filter_ctx);
        RedisModule_Free(filter_expression);
    }

    return err;
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
        const Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetadata *metadata,
        struct Selva_SubscriptionMarker *marker) {
    struct Selva_SubscriptionMarkers *sub_markers = &metadata->sub_markers;

    /* Detached markers are never set directly on the nodes */
    if (marker->marker_flags & SELVA_SUBSCRIPTION_FLAG_DETACH) {
        return;
    }

    switch (marker->dir) {
    case SELVA_HIERARCHY_TRAVERSAL_NONE:
        /* NOP */
        break;
    case SELVA_HIERARCHY_TRAVERSAL_NODE:
        /* fall through */
        __attribute__((fallthrough));
    case SELVA_HIERARCHY_TRAVERSAL_CHILDREN:
        /* fall through */
        __attribute__((fallthrough));
    case SELVA_HIERARCHY_TRAVERSAL_PARENTS:
        /* These traversal direction types are only copied/set on the node itself. */
        if (memcmp(marker->node_id, node_id, SELVA_NODE_ID_SIZE) != 0) {
            break;
        }
        /* fall through */
        __attribute__((fallthrough));
    default:
        set_marker(sub_markers, marker);
        break;
    }
}


static int refreshSubscription(struct SelvaModify_Hierarchy *hierarchy, struct Selva_Subscription *sub) {
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;
    int res = 0;

    assert(sub);

    SVector_ForeachBegin(&it, &sub->markers);
    while ((marker = SVector_Foreach(&it))) {
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
            .node_cb = set_node_marker_cb,
            .node_arg = marker,
        };

        /*
         * This could be implemented with a head callback but it's not
         * currently implemented for the traverse API. Therefore we do a
         * separate traverse just for the node itself in some special cases
         * where it's necessary.
         */
        if (marker->dir == SELVA_HIERARCHY_TRAVERSAL_PARENTS || marker->dir == SELVA_HIERARCHY_TRAVERSAL_CHILDREN) {
            err = SelvaModify_TraverseHierarchy(hierarchy, marker->node_id, SELVA_HIERARCHY_TRAVERSAL_NODE, &cb);
        } else {
            err = 0;
        }
        if (!err) {
            err = SelvaModify_TraverseHierarchy(hierarchy, marker->node_id, marker->dir, &cb);
        }
        if (err) {
#if 0
            char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

            fprintf(stderr, "%s: Could not fully apply a subscription: %s:%d err: %s\n",
                    __FILE__,
                    Selva_SubscriptionId2str(str, sub->sub_id), marker->marker_id,
                    getSelvaErrorStr(err));
#endif

            /*
             * Don't report ENOENT errors because subscriptions are valid for
             * non-existent nodeIds.
             */
            if (err != SELVA_MODIFY_HIERARCHY_ENOENT) {
                res = err; /* Report the last error */
            }
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
 * Clear subscription starting from node_id and remove the subscription if
 * node_id is the starting point.
 */
static void clear_sub(struct SelvaModify_Hierarchy *hierarchy, struct Selva_SubscriptionMarker *marker, Selva_NodeId node_id) {
    struct SelvaModify_HierarchyCallback cb = {
        .node_cb = clear_node_marker_cb,
        .node_arg = marker,
    };

    /*
     * Remove subscription markers.
     */
    (void)SelvaModify_TraverseHierarchy(hierarchy, node_id, SELVA_HIERARCHY_TRAVERSAL_NODE, &cb);
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
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;
    svector_autofree SVector markers = {0};

    if (nr_markers == 0) {
        return;
    }

#if 0
    fprintf(stderr, "Removing %zu subscription markers from %.*s\n",
            nr_markers, (int)SELVA_NODE_ID_SIZE, node_id);
#endif

    if (unlikely(!SVector_Clone(&markers, &metadata->sub_markers.vec, NULL))) {
        fprintf(stderr, "%s: %s ENOMEM\n", __FILE__, __func__);
        return;
    }

    /*
     * Remove each subscription marker from this node and its ancestors/descendants.
     */
    SVector_ForeachBegin(&it, &markers);
    while ((marker = SVector_Foreach(&it))) {
        clear_sub(hierarchy, marker, node_id);
    }
    SVector_Clear(&metadata->sub_markers.vec);
}

int SelvaSubscriptions_InitDeferredEvents(struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    return !SVector_Init(&def->updates, 2, SelvaSubscription_svector_compare) ||
           !SVector_Init(&def->triggers, 3, marker_svector_compare)
           ? SELVA_SUBSCRIPTIONS_ENOMEM : 0;
}

void SelvaSubscriptions_DestroyDeferredEvents(struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    if (!def) {
        return;
    }

    SVector_Destroy(&def->updates);
    SVector_Destroy(&def->triggers);
}

void SelvaSubscriptions_InheritParent(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *node_metadata,
        size_t node_nr_children,
        const Selva_NodeId parent_id,
        struct SelvaModify_HierarchyMetadata *parent_metadata) {
    /*
     * Trigger all relevant subscriptions to make sure the subscriptions are
     * propagated properly.
     * If the root node is a parent for this node then we'll just trigger
     * everything for now.
     */
    if (node_nr_children > 0 || !memcmp(parent_id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE)) {
        SelvaSubscriptions_DeferHierarchyEvents(hierarchy, parent_id, parent_metadata);
    } else {
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;
        struct Selva_SubscriptionMarkers *node_sub_markers = &node_metadata->sub_markers;

        SVector_ForeachBegin(&it, &parent_metadata->sub_markers.vec);
        while ((marker = SVector_Foreach(&it))) {
            if (!isHierarchyMarker(marker->marker_flags)) {
                continue;
            }

            switch (marker->dir) {
            case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
            case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
            case SELVA_HIERARCHY_TRAVERSAL_DFS_FULL:
                /* These markers can be copied safely. */
                set_marker(node_sub_markers, marker);
                break;
            default:
                /*
                 * There is no fast & reliable way to figure out what to do with
                 * the other types so we'll just send a generic hierarchy event.
                 */
                SelvaSubscriptions_DeferHierarchyEvents(hierarchy, parent_id, parent_metadata);
                break;
            }
        }
    }
}

void SelvaSubscriptions_InheritChild(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *node_metadata,
        size_t node_nr_parents,
        const Selva_NodeId child_id,
        struct SelvaModify_HierarchyMetadata *child_metadata) {
    /*
     * Trigger all relevant subscriptions to make sure the subscriptions are
     * propagated properly.
     */
    if (node_nr_parents > 0) {
        SelvaSubscriptions_DeferHierarchyEvents(hierarchy, child_id, child_metadata);
    } else {
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;
        struct Selva_SubscriptionMarkers *node_sub_markers = &node_metadata->sub_markers;

        SVector_ForeachBegin(&it, &child_metadata->sub_markers.vec);
        while ((marker = SVector_Foreach(&it))) {
            if (!isHierarchyMarker(marker->marker_flags)) {
                continue;
            }

            switch (marker->dir) {
            case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
            case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
                /* These markers can be copied safely. */
                set_marker(node_sub_markers, marker);
                break;
            default:
                /*
                 * There is no fast & reliable way to figure out what to do with
                 * the other types so we'll just send a generic hierarchy event.
                 */
                SelvaSubscriptions_DeferHierarchyEvents(hierarchy, child_id, child_metadata);
                break;
            }
        }
    }
}

static int isSubscribedToHierarchyFields(struct Selva_SubscriptionMarker *marker) {
    int res;

    res = Selva_SubscriptionFieldMatch(marker, "ancestors") ||
          Selva_SubscriptionFieldMatch(marker, "children") ||
          Selva_SubscriptionFieldMatch(marker, "descendants") ||
          Selva_SubscriptionFieldMatch(marker, "parents");

    return res;
}

static void defer_update_event(struct SelvaSubscriptions_DeferredEvents *def, struct Selva_Subscription *sub) {
    SVector_InsertFast(&def->updates, sub);
}

static void defer_trigger_event(struct SelvaSubscriptions_DeferredEvents *def, struct Selva_SubscriptionMarker *marker) {
    SVector_InsertFast(&def->triggers, marker);
}

/**
 * Defer events for missing accessor signaling creation of nodes and aliases.
 * @param id nodeId or alias.
 */
void SelvaSubscriptions_DeferMissingAccessorEvents(struct SelvaModify_Hierarchy *hierarchy, const char *id_str, size_t id_len) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    struct Selva_Subscription *sub;
    SelvaObject_Iterator *it;
    struct SelvaObject *obj;
    int err;

    if (SelvaObject_ExistsStr(hierarchy->subs.missing, id_str, id_len)) {
        return;
    }

    /* Get the <id> object containing a number of subscription pointers for this id. */
    err = SelvaObject_GetObjectStr(hierarchy->subs.missing, id_str, id_len, &obj);
    if (err) {
        fprintf(stderr, "%s: Failed to get missing accessor marker: %s", __FILE__, getSelvaErrorStr(err));
        return;
    }

    /* Defer event for each subscription. */
    it = SelvaObject_ForeachBegin(obj);
    while ((sub = (struct Selva_Subscription *)SelvaObject_ForeachValue(obj, &it, NULL, SELVA_OBJECT_POINTER))) {
        defer_update_event(def, sub);
    }

    /* Finally delete the ID as all events were deferred. */
    SelvaObject_DelKeyStr(obj, id_str, id_len);
}

static void defer_hierarchy_events(struct SelvaModify_Hierarchy *hierarchy,
                                   const Selva_NodeId node_id __unused,
                                   const struct Selva_SubscriptionMarkers *sub_markers) {
    if (isHierarchyMarker(sub_markers->flags_filter)) {
        struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;

        SVector_ForeachBegin(&it, &sub_markers->vec);
        while ((marker = SVector_Foreach(&it))) {
            /*
             * We cannot call inhibitMarkerEvent() here because the client needs
             * to refresh the subscription even if this node is not part of the
             * marker filter.
             * E.g. consider subscription marker for children of a node. In this
             * case we need to apply the marker to any new children.
             */
            if (isHierarchyMarker(marker->marker_flags) &&
                /*
                 * SELVA_HIERARCHY_TRAVERSAL_NODE doesn't need to send an event
                 * if there is no subscription to hierarchy fields because the
                 * marker will never need a refresh.
                 */
                (marker->dir != SELVA_HIERARCHY_TRAVERSAL_NODE || isSubscribedToHierarchyFields(marker))) {
                defer_update_event(def, marker->sub);
            }
        }
    }
}

void SelvaSubscriptions_DeferHierarchyEvents(struct SelvaModify_Hierarchy *hierarchy,
                                             const Selva_NodeId node_id,
                                             const struct SelvaModify_HierarchyMetadata *metadata) {
    /* Detached markers. */
    defer_hierarchy_events(hierarchy, node_id, &hierarchy->subs.detached_markers);

    if (!metadata) {
        fprintf(stderr, "%s:%d Node metadata missing %.*s\n",
                __FILE__, __LINE__, (int)SELVA_NODE_ID_SIZE, node_id);
        return;
    }

    /* Markers on the node. */
    defer_hierarchy_events(hierarchy, node_id, &metadata->sub_markers);
}

static void defer_hierarchy_deletion_events(struct SelvaModify_Hierarchy *hierarchy,
                                            const Selva_NodeId node_id __unused,
                                            const struct Selva_SubscriptionMarkers *sub_markers) {
    if (isHierarchyMarker(sub_markers->flags_filter)) {
        struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;

        SVector_ForeachBegin(&it, &sub_markers->vec);
        while ((marker = SVector_Foreach(&it))) {
            /*
             * Deletions are always sent to all hierarchy markers regardless of
             * field subscriptions or inhibits.
             */
            if (isHierarchyMarker(marker->marker_flags)) {
                defer_update_event(def, marker->sub);
            }
        }
    }
}

void SelvaSubscriptions_DeferHierarchyDeletionEvents(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata) {
    /* Detached markers. */
    defer_hierarchy_deletion_events(hierarchy, node_id, &hierarchy->subs.detached_markers);

    if (!metadata) {
        fprintf(stderr, "%s:%d Node metadata missing %.*s\n",
                __FILE__, __LINE__, (int)SELVA_NODE_ID_SIZE, node_id);
        return;
    }

    /* Markers on the node. */
    defer_hierarchy_deletion_events(hierarchy, node_id, &metadata->sub_markers);
}

static void defer_alias_change_events(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        const struct Selva_SubscriptionMarkers *sub_markers,
        const Selva_NodeId node_id,
        SVector *wipe_subs) {
    if (!isAliasMarker(sub_markers->flags_filter)) {
        /* No alias markers in this structure. */
        return;
    }

    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;

    SVector_ForeachBegin(&it, &sub_markers->vec);
    while ((marker = SVector_Foreach(&it))) {
        if (isAliasMarker(marker->marker_flags) &&
            /* The filter should contain `in` matcher for the alias. */
            Selva_SubscriptionFilterMatch(ctx, node_id, marker)
            ) {
            defer_update_event(def, marker->sub);

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
        RedisModuleCtx *ctx,
        const Selva_NodeId node_id,
        const struct Selva_SubscriptionMarkers *sub_markers) {
    const unsigned short flags = SELVA_SUBSCRIPTION_FLAG_CH_FIELD;

    if ((sub_markers->flags_filter & flags) == flags) {
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;

        SVector_ForeachBegin(&it, &sub_markers->vec);
        while ((marker = SVector_Foreach(&it))) {
            if (((marker->marker_flags & flags) == flags)) {
                /*
                 * Store the filter result before any changes to the node.
                 * We assume that SelvaSubscriptions_DeferFieldChangeEvents()
                 * is called before this function is called for another node.
                 */
                memcpy(marker->filter_history.node_id, node_id, SELVA_NODE_ID_SIZE);
                marker->filter_history.res = Selva_SubscriptionFilterMatch(ctx, node_id, marker);
            }
        }
    }
}

void SelvaSubscriptions_FieldChangePrecheck(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata) {
    /* Detached markers. */
    field_change_precheck(ctx, node_id, &hierarchy->subs.detached_markers);

    if (!metadata) {
        fprintf(stderr, "%s:%d Node metadata missing\n", __FILE__, __LINE__);
        return;
    }

    /* Markers on the node. */
    field_change_precheck(ctx, node_id, &metadata->sub_markers);
}

static void defer_field_change_events(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct Selva_SubscriptionMarkers *sub_markers,
        const char *field) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    const unsigned short flags = SELVA_SUBSCRIPTION_FLAG_CH_FIELD;

    if ((sub_markers->flags_filter & flags) == flags) {
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;

        SVector_ForeachBegin(&it, &sub_markers->vec);
        while ((marker = SVector_Foreach(&it))) {
            if (((marker->marker_flags & flags) == flags) && !inhibitMarkerEvent(node_id, marker)) {
                const int expressionMatchBefore = (marker->filter_history.res && !memcmp(marker->filter_history.node_id, node_id, SELVA_NODE_ID_SIZE));
                const int expressionMatchAfter = Selva_SubscriptionFilterMatch(ctx, node_id, marker);
                const int fieldsMatch = Selva_SubscriptionFieldMatch(marker, field);

                if ((expressionMatchBefore && expressionMatchAfter && fieldsMatch) || (expressionMatchBefore ^ expressionMatchAfter)) {
                    defer_update_event(def, marker->sub);
                }
            }
        }
    }
}

void SelvaSubscriptions_DeferFieldChangeEvents(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata,
        const char *field) {
    /* Detached markers. */
    defer_field_change_events(ctx, hierarchy, node_id, &hierarchy->subs.detached_markers, field);

    if (!metadata) {
        fprintf(stderr, "%s:%d Node metadata missing\n", __FILE__, __LINE__);
        return;
    }

    /* Markers on the node. */
    defer_field_change_events(ctx, hierarchy, node_id, &metadata->sub_markers, field);
}

/**
 * Defer alias events and wipeout markers of the subscriptions hit.
 */
void Selva_Subscriptions_DeferAliasChangeEvents(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        RedisModuleString *alias_name) {
    svector_autofree SVector wipe_subs;
    Selva_NodeId orig_node_id;
    struct SelvaModify_HierarchyMetadata *orig_metadata;
    int err;

    SVector_Init(&wipe_subs, 0, SelvaSubscription_svector_compare);

    err = SelvaResolve_NodeId(ctx, hierarchy, (RedisModuleString *[]){ alias_name }, 1, orig_node_id);
    if (err < 0) {
        return;
    }
    orig_metadata = SelvaModify_HierarchyGetNodeMetadata(hierarchy, orig_node_id);
    if (!orig_metadata) {
        fprintf(stderr, "%s: Failed to get metadata for node: \"%.*s\"\n",
                __FILE__, (int)SELVA_NODE_ID_SIZE, orig_node_id);
        return;
    }

    /*
     * Alias markers are never detached so no need to handle those.
     */

    /* Defer events for markers on the src node. */
    defer_alias_change_events(
            ctx,
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

void Selva_Subscriptions_DeferTriggerEvents(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        enum Selva_SubscriptionTriggerType event_type) {
    /* Trigger markers are always detached and have no node_id. */
    const struct Selva_SubscriptionMarkers *sub_markers = &hierarchy->subs.detached_markers;

    if (isTriggerMarker(sub_markers->flags_filter)) {
        struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
        struct SVectorIterator it;
        struct Selva_SubscriptionMarker *marker;

        SVector_ForeachBegin(&it, &sub_markers->vec);
        while ((marker = SVector_Foreach(&it))) {
            if (isTriggerMarker(marker->marker_flags) &&
                marker->event_type == event_type &&
                Selva_SubscriptionFilterMatch(ctx, node_id, marker)) {
                /*
                 * The node_id might be there already if the marker has a filter
                 * but trigger events will need the node_id there regardless of if
                 * a filter is actually used.
                 */
                memcpy(marker->filter_history.node_id, node_id, SELVA_NODE_ID_SIZE);

                defer_trigger_event(def, marker);
            }
        }
    }
}

static void send_update_events(struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    struct SVectorIterator it;
    struct Selva_Subscription *sub;

    SVector_ForeachBegin(&it, &def->updates);
    while ((sub = SVector_Foreach(&it))) {
#if 0
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

        fprintf(stderr, "%s: publish update event %s\n", __FILE__, Selva_SubscriptionId2str(str, sub->sub_id));
#endif
        SelvaModify_PublishSubscriptionUpdate(sub->sub_id);
    }
    SVector_Clear(&def->updates);
}

static void send_trigger_events(struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;

    SVector_ForeachBegin(&it, &def->triggers);
    while ((marker = SVector_Foreach(&it))) {
#if 0
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];

        fprintf(stderr, "%s: publish trigger event %s\n", __FILE__, Selva_SubscriptionId2str(str, marker->sub->sub_id));
#endif

        SelvaModify_PublishSubscriptionTrigger(marker->sub->sub_id, marker->filter_history.node_id);
    }
    SVector_Clear(&def->triggers);
}

void SelvaSubscriptions_SendDeferredEvents(struct SelvaModify_Hierarchy *hierarchy) {
    send_update_events(hierarchy);
    send_trigger_events(hierarchy);
}

static int parse_subscription_type(enum SelvaModify_HierarchyTraversal *dir, RedisModuleString *arg) {
    TO_STR(arg)

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
    } else if (arg_len > 0) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_REF;
    } else {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    return 0;
}

/*
 * KEY SUB_ID MARKER_ID node|ancestors|descendants|ref_field_name NODE_ID [fields <fieldnames \n separated>] [filter expression] [filter args...]
 */
int Selva_AddMarkerCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_REDIS_KEY     = 1;
    const int ARGV_SUB_ID        = 2;
    const int ARGV_MARKER_ID     = 3;
    const int ARGV_MARKER_TYPE   = 4;
    const int ARGV_NODE_ID       = 5;
    const int ARGV_FIELDS        = 6;
    const int ARGV_FIELD_NAMES   = 7;
    int ARGV_FILTER_EXPR         = 6;
    int ARGV_FILTER_ARGS         = 7;
#define SHIFT_ARGS(i) \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    if (argc < ARGV_NODE_ID + 1) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = SelvaArgParser_SubscriptionId(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Subscription ID");
    }

    /*
     * Get the marker id.
     */
    Selva_SubscriptionMarkerId marker_id;
    err = SelvaArgParser_MarkerId(&marker_id, argv[ARGV_MARKER_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Marker ID");
    }

    if (SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id)) {
        /* Marker already created. */
        return RedisModule_ReplyWithLongLong(ctx, 1);
    }

    /*
     * Parse the traversal argument.
     */
    enum SelvaModify_HierarchyTraversal sub_dir;
    const char *ref_field = NULL;
    err = parse_subscription_type(&sub_dir, argv[ARGV_MARKER_TYPE]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Traversal argument");
    }
    if (sub_dir == SELVA_HIERARCHY_TRAVERSAL_REF) {
        /* The arg is a field name. */
        ref_field = RedisModule_StringPtrLen(argv[ARGV_MARKER_TYPE], NULL);
    }

    /*
     * Get the nodeId.
     */
    Selva_NodeId node_id;
    SelvaArgParser_NodeId(node_id, argv[ARGV_NODE_ID]);

    /*
     * Get field names for change events.
     * Optional.
     */
    const char *fields = NULL;
    if (argc > ARGV_FIELD_NAMES) {
        err = SelvaArgParser_StrOpt(&fields, "fields", argv[ARGV_FIELDS], argv[ARGV_FIELD_NAMES]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "Fields");
        }
    }

    /*
     * Parse & compile the filter expression.
     * Optional.
     */
    struct rpn_ctx *filter_ctx = NULL;
    rpn_token *filter_expression = NULL;
    if (argc >= ARGV_FILTER_EXPR + 1) {
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
        const char *input;
        size_t input_len;

        filter_ctx = rpn_init(nr_reg);
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
            str = RedisModule_StringPtrLen(argv[i], &str_len);
            str_len++;
            arg = RedisModule_Alloc(str_len);
            memcpy(arg, str, str_len);

            rpn_set_reg(filter_ctx, reg_i, arg, str_len, RPN_SET_REG_FLAG_RMFREE);
        }
    }

    unsigned short marker_flags = 0;

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
        err = Selva_AddSubscriptionMarker(hierarchy, sub_id, marker_id, marker_flags, "ndfer",
                                          node_id, sub_dir, fields,
                                          filter_ctx, filter_expression,
                                          ref_field);
    } else {
        err = Selva_AddSubscriptionMarker(hierarchy, sub_id, marker_id, marker_flags, "nder",
                                          node_id, sub_dir,
                                          filter_ctx, filter_expression,
                                          ref_field);
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
    } else { /* Error */
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

    const int ARGV_REDIS_KEY     = 1;
    const int ARGV_SUB_ID        = 2;
    const int ARGV_MARKER_ID     = 3;
    const int ARGV_FIELD_NAMES   = 4;

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = SelvaArgParser_SubscriptionId(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Subscription ID");
    }

    /*
     * Get the marker id.
     */
    Selva_SubscriptionMarkerId marker_id;
    err = SelvaArgParser_MarkerId(&marker_id, argv[ARGV_MARKER_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Marker ID");
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
 * KEY SUB_ID MARKER_ID ALIAS_NAME
 */
int Selva_SubscribeAliasCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_REDIS_KEY     = 1;
    const int ARGV_SUB_ID        = 2;
    const int ARGV_MARKER_ID     = 3;
    const int ARGV_ALIAS_NAME    = 4;

    if (argc < ARGV_ALIAS_NAME + 1) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = SelvaArgParser_SubscriptionId(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Subscription ID");
    }

    /*
     * Get the marker id.
     */
    Selva_SubscriptionMarkerId marker_id;
    err = SelvaArgParser_MarkerId(&marker_id, argv[ARGV_MARKER_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Marker ID");
    }

    /*
     * Get the alias name.
     */
    RedisModuleString *alias_name = argv[ARGV_ALIAS_NAME];

    /*
     * Resolve the node_id as we want to apply the marker
     * on the node the alias is pointing to.
     */
    Selva_NodeId node_id;
    err = SelvaResolve_NodeId(ctx, hierarchy, argv + ARGV_ALIAS_NAME, 1, node_id);
    if (err < 0) {
        return replyWithSelvaError(ctx, err);
    }

    err = Selva_AddSubscriptionAliasMarker(hierarchy, sub_id, marker_id, alias_name, node_id);
    if (err) {
        replyWithSelvaError(ctx, err);
    } else {
        RedisModule_ReplyWithLongLong(ctx, 1);
#if 0
        RedisModule_ReplicateVerbatim(ctx);
#endif
    }

    return REDISMODULE_OK;
}

/**
 * Add missing node/alias markers.
 * SELVA.SUBSCRIPTIONS.ADDMISSING KEY SUB_ID NODEID|ALIAS...
 */
int Selva_AddMissingCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    int err;

    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

    const int ARGV_REDIS_KEY = 1;
    const int ARGV_SUB_ID    = 2;
    const int ARGV_IDS       = 3;

    Selva_SubscriptionId sub_id;
    err = SelvaArgParser_SubscriptionId(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Invalid Subscription ID");
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    /*
     * Open the subscription.
     */
    struct Selva_Subscription *sub;
    sub = find_sub(hierarchy, sub_id);
    if (!sub) {
        sub = create_subscription(hierarchy, sub_id);
        if (!sub) {
            return replyWithSelvaError(ctx, SELVA_SUBSCRIPTIONS_ENOMEM);
        }
    }

    long long n = 0;
    for (int i = ARGV_IDS; i < argc; i++) {
        char sub_id_str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
        RedisModuleString *missing;

        missing = RedisModule_CreateStringPrintf(
            ctx, "%s.%.*s",
            RedisModule_StringPtrLen(argv[i], NULL),
            (int)SELVA_SUBSCRIPTION_ID_STR_LEN, Selva_SubscriptionId2str(sub_id_str, sub_id));
        if (!missing) {
            err = SELVA_SUBSCRIPTIONS_ENOMEM;
            return replyWithSelvaErrorf(ctx, err, "Creating missing markers");
        }

        err = SelvaObject_SetPointer(hierarchy->subs.missing, missing, sub);
        if (err) {
            return replyWithSelvaError(ctx, err);
        }

        n++;
    }

    return RedisModule_ReplyWithLongLong(ctx, n);
}

/**
 * Add a trigger marker.
 * SELVA.SUBSCRIPTIONS.ADDTRIGGER KEY SUB_ID MARKER_ID EVENT_TYPE [filter expression] [filter args...]
 */
int Selva_AddTriggerCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_REDIS_KEY     = 1;
    const int ARGV_SUB_ID        = 2;
    const int ARGV_MARKER_ID     = 3;
    const int ARGV_EVENT_TYPE    = 4;
    int ARGV_FILTER_EXPR         = 5;
    int ARGV_FILTER_ARGS         = 6;

    if (argc < ARGV_EVENT_TYPE + 1) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = SelvaArgParser_SubscriptionId(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Subscription ID");
    }

    /*
     * Get the marker id.
     */
    Selva_SubscriptionMarkerId marker_id;
    err = SelvaArgParser_MarkerId(&marker_id, argv[ARGV_MARKER_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Marker ID");
    }

    /* Parse event_type */
    err = SelvaArgParser_Enum(trigger_event_types, argv[ARGV_EVENT_TYPE]);
    if (err < 0) {
        return replyWithSelvaErrorf(ctx, err, "Event type");
    }
    const enum Selva_SubscriptionTriggerType event_type = err;

    if (SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id)) {
        /* Marker already created. */
        return RedisModule_ReplyWithLongLong(ctx, 1);
    }

    /*
     * Parse & compile the filter expression.
     * Optional.
     */
    struct rpn_ctx *filter_ctx = NULL;
    rpn_token *filter_expression = NULL;
    if (argc >= ARGV_FILTER_EXPR + 1) {
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
        const char *input;
        size_t input_len;

        filter_ctx = rpn_init(nr_reg);
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
            str = RedisModule_StringPtrLen(argv[i], &str_len);
            str_len++;
            arg = RedisModule_Alloc(str_len);
            memcpy(arg, str, str_len);

            rpn_set_reg(filter_ctx, reg_i, arg, str_len, RPN_SET_REG_FLAG_RMFREE);
        }
    }

    const unsigned short marker_flags = SELVA_SUBSCRIPTION_FLAG_DETACH | SELVA_SUBSCRIPTION_FLAG_TRIGGER;
    err = Selva_AddSubscriptionMarker(hierarchy, sub_id, marker_id, marker_flags, "Ee",
                                      event_type,
                                      filter_ctx, filter_expression);
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
    } else { /* Error */
out:
        if (filter_ctx) {
            rpn_destroy(filter_ctx);
            RedisModule_Free(filter_expression);
        }
        replyWithSelvaError(ctx, err);
    }

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
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    Selva_SubscriptionId sub_id;
    err = SelvaArgParser_SubscriptionId(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        fprintf(stderr, "%s:%d: Invalid sub_id \"%s\"\n", __FILE__, __LINE__, RedisModule_StringPtrLen(argv[ARGV_SUB_ID], NULL));
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

/**
 * List all subscriptions.
 * KEY
 */
int Selva_SubscriptionsListCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    const int ARGV_REDIS_KEY = 1;

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        /* Do not send redis messages here. */
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

int Selva_SubscriptionsListMissingCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    int err;

    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    const int ARGV_REDIS_KEY = 1;

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    err = SelvaObject_ReplyWithObject(ctx, hierarchy->subs.missing, NULL);
    if (err) {
        replyWithSelvaError(ctx, err);
    }

    return REDISMODULE_OK;
}

/*
 * KEY SUB_ID
 */
int Selva_SubscriptionDebugCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    const int ARGV_REDIS_KEY = 1;
    const int ARGV_ID        = 2;

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    size_t id_len = 0;
    const char *id_str = RedisModule_StringPtrLen(argv[ARGV_ID], &id_len);

    const int is_sub_id = id_len == SELVA_SUBSCRIPTION_ID_STR_LEN;
    const int is_node_id = id_len <= SELVA_NODE_ID_SIZE;
    SVector *markers = NULL;

    if (is_sub_id) {
        int err;
        Selva_SubscriptionId sub_id;
        struct Selva_Subscription *sub;

        err = SelvaArgParser_SubscriptionId(sub_id, argv[ARGV_ID]);
        if (err) {
            fprintf(stderr, "%s:%d: Invalid sub_id \"%s\"\n", __FILE__, __LINE__, RedisModule_StringPtrLen(argv[ARGV_ID], NULL));
            return replyWithSelvaError(ctx, err);
        }

        sub = find_sub(hierarchy, sub_id);
        if (!sub) {
            return replyWithSelvaError(ctx, SELVA_SUBSCRIPTIONS_ENOENT);
        }

        markers = &sub->markers;
    } else if (is_node_id) {
        if (!memcmp("detached", id_str, id_len)) {
            markers = &hierarchy->subs.detached_markers.vec;
        } else {
            Selva_NodeId node_id;

            Selva_NodeIdCpy(node_id, id_str);
            struct SelvaModify_HierarchyMetadata *metadata;

            metadata = SelvaModify_HierarchyGetNodeMetadata(hierarchy, node_id);
            if (!metadata) {
                return replyWithSelvaError(ctx, SELVA_SUBSCRIPTIONS_ENOENT);
            }

            markers = &metadata->sub_markers.vec;
        }
    } else {
        return replyWithSelvaError(ctx, SELVA_SUBSCRIPTIONS_EINVAL);
    }


    size_t array_len = 0;
    struct SVectorIterator it;
    struct Selva_SubscriptionMarker *marker;

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    SVector_ForeachBegin(&it, markers);
    while ((marker = SVector_Foreach(&it))) {
        char sub_buf[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
        const int is_trigger = isTriggerMarker(marker->marker_flags);

        RedisModule_ReplyWithArray(ctx, is_trigger ? 5 : 7);
        RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "sub_id: %s", Selva_SubscriptionId2str(sub_buf, marker->sub->sub_id)));
        RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "marker_id: %d", (int)marker->marker_id));
        RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "flags: %#06x", marker->marker_flags));
        if (is_trigger) {
            RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "event_type: %s", trigger_event_types[marker->event_type]));
        } else {
            RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "node_id: \"%.*s\"", (int)SELVA_NODE_ID_SIZE, marker->node_id));
            RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "dir: %s", SelvaModify_HierarchyDir2str(marker->dir)));
        }
        RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "filter_expression: %s", (marker->filter_ctx) ? "set" : "unset"));
        if (!is_trigger) {
            RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "fields: \"%s\"", marker->fields ? marker->fields : "(null)"));
        }

        array_len++;
    }
    RedisModule_ReplySetArrayLength(ctx, array_len);

    return REDISMODULE_OK;
}

/*
 * KEY SUB_ID
 */
int Selva_DelCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    int err;

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    const int ARGV_REDIS_KEY = 1;
    const int ARGV_SUB_ID    = 2;

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    Selva_SubscriptionId sub_id;
    err = SelvaArgParser_SubscriptionId(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        fprintf(stderr, "%s:%d: Invalid sub_id \"%s\"\n", __FILE__, __LINE__, RedisModule_StringPtrLen(argv[ARGV_SUB_ID], NULL));
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
     * All commands are marked readonly because they don't change the
     * observed or serialized key values in any way. This is important
     * because we need to be able to create markers on readonly replicas.
     */
    if (RedisModule_CreateCommand(ctx, "selva.subscriptions.add", Selva_AddMarkerCommand, "readonly deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.addMarkerFields", Selva_AddSubscriptionMarkerFieldsCommand, "readonly deny-oom", 1, 1 ,1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.addAlias", Selva_SubscribeAliasCommand, "readonly deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.addMissing", Selva_AddMissingCommand, "readonly deny-oom", 1, 1 ,1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.addTrigger", Selva_AddTriggerCommand, "readonly deny-oom", 1, 1 ,1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.refresh", Selva_RefreshSubscriptionCommand, "readonly deny-oom", 1, 1 ,1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.list", Selva_SubscriptionsListCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.listMissing", Selva_SubscriptionsListMissingCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.debug", Selva_SubscriptionDebugCommand, "readonly deny-script", 1, 1, 1) ||
        RedisModule_CreateCommand(ctx, "selva.subscriptions.del", Selva_DelCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Hierarchy_Subscriptions_OnLoad);
