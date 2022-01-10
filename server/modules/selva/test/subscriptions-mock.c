#include "cdefs.h"
#include "selva.h"
#include "errors.h"
#include "hierarchy.h"
#include "subscriptions.h"
#include "selva_object.h"

struct SelvaHierarchy;
struct SelvaHierarchyMetadata;

int Selva_Subscriptions_InitHierarchy(struct SelvaHierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    hierarchy->subs.missing = SelvaObject_New();
    SVector_Init(&def->updates, 2, NULL);
    SVector_Init(&def->triggers, 3, NULL);
    SVector_Init(&hierarchy->subs.detached_markers.vec, 0, NULL);
    hierarchy->subs.detached_markers.flags_filter = 0;

    return 0;
}

void SelvaSubscriptions_DestroyAll(struct RedisModuleCtx *ctx, struct SelvaHierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    SVector_Destroy(&def->updates);
    SVector_Destroy(&def->triggers);
    SelvaObject_Destroy(hierarchy->subs.missing);
}

void SelvaSubscriptions_ClearAllMarkers(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node) {
    return;
}

int SelvaSubscriptions_hasActiveMarkers(const struct SelvaHierarchyMetadata *node_metadata) {
    return 0;
}

void SelvaSubscriptions_InheritParent(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        struct SelvaHierarchyMetadata *node_metadata,
        size_t node_nr_children,
        struct SelvaHierarchyNode *parent) {
    return;
}

void SelvaSubscriptions_InheritChild(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        struct SelvaHierarchyMetadata *node_metadata,
        size_t node_nr_parents,
        struct SelvaHierarchyNode *child) {
    return;
}

void SelvaSubscriptions_InheritEdge(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *src_node,
        struct SelvaHierarchyNode *dst_node,
        const char *field_str,
        size_t field_len) {
    return;
}

int SelvaSubscriptions_DeleteMarker(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_SubscriptionId sub_id,
        Selva_SubscriptionMarkerId marker_id) {
    return 0;
}
int SelvaSubscriptions_DeleteMarkerByPtr(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct Selva_SubscriptionMarker *marker) {
    return 0;
}

void SelvaSubscriptions_DeferMissingAccessorEvents(struct SelvaHierarchy *hierarchy, const char *id_str, size_t id_len) {
    return;
}

void SelvaSubscriptions_DeferFieldChangeEvents(
        struct RedisModuleCtx *redis_ctx,
        struct SelvaHierarchy *hierarchy,
        const struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len) {
    return;
}

void SelvaSubscriptions_DeferHierarchyEvents(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const struct SelvaHierarchyNode *node) {
    return;
}

void SelvaSubscriptions_DeferHierarchyDeletionEvents(
        struct RedisModuleCtx *redis_ctx,
        struct SelvaHierarchy *hierarchy,
        const struct SelvaHierarchyNode *node) {
    return;
}

void Selva_Subscriptions_DeferTriggerEvents(
        struct RedisModuleCtx *redis_ctx,
        struct SelvaHierarchy *hierarchy,
        const struct SelvaHierarchyNode *node,
        enum Selva_SubscriptionTriggerType event_type) {
    return;
}

void SelvaSubscriptions_RefreshByMarker(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const SVector *markers) {
    return;
}

void SelvaSubscriptions_SendDeferredEvents(struct SelvaHierarchy *hierarchy) {
    return;
}
