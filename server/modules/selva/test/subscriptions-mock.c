#include "cdefs.h"
#include "selva.h"
#include "errors.h"
#include "hierarchy.h"
#include "subscriptions.h"
#include "selva_object.h"

struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyMetadata;

int Selva_Subscriptions_InitHierarchy(struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    hierarchy->subs.missing = SelvaObject_New();
    SVector_Init(&def->updates, 2, NULL);
    SVector_Init(&def->triggers, 3, NULL);
    SVector_Init(&hierarchy->subs.detached_markers.vec, 0, NULL);
    hierarchy->subs.detached_markers.flags_filter = 0;

    return 0;
}

void SelvaSubscriptions_DestroyAll(struct RedisModuleCtx *ctx __unused, struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    SVector_Destroy(&def->updates);
    SVector_Destroy(&def->triggers);
    SelvaObject_Destroy(hierarchy->subs.missing);
}
void SelvaSubscriptions_ClearAllMarkers(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        struct SelvaModify_HierarchyNode *node __unused) {
    return;
}

void SelvaSubscriptions_InheritParent(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *node_metadata __unused,
        size_t node_nr_children __unused,
        struct SelvaModify_HierarchyNode *parent __unused) {
    return;
}

void SelvaSubscriptions_InheritChild(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *node_metadata __unused,
        size_t node_nr_parents __unused,
        struct SelvaModify_HierarchyNode *child __unused) {
    return;
}

void SelvaSubscriptions_InheritEdge(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        struct SelvaModify_HierarchyNode *src_node __unused,
        struct SelvaModify_HierarchyNode *dst_node __unused,
        const char *field_str __unused,
        size_t field_len __unused) {
    return;
}

int SelvaSubscriptions_DeleteMarker(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_SubscriptionId sub_id __unused,
        Selva_SubscriptionMarkerId marker_id __unused) {
    return 0;
}
int SelvaSubscriptions_DeleteMarkerByPtr(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        struct Selva_SubscriptionMarker *marker __unused) {
    return 0;
}

void SelvaSubscriptions_DeferMissingAccessorEvents(struct SelvaModify_Hierarchy *hierarchy __unused, const char *id_str __unused, size_t id_len __unused) {
    return;
}

void SelvaSubscriptions_DeferFieldChangeEvents(
        struct RedisModuleCtx *redis_ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const struct SelvaModify_HierarchyNode *node __unused,
        const char *field_str __unused,
        size_t field_len __unused) {
    return;
}

void SelvaSubscriptions_DeferHierarchyEvents(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const struct SelvaModify_HierarchyNode *node __unused) {
    return;
}

void SelvaSubscriptions_DeferHierarchyDeletionEvents(
        struct RedisModuleCtx *redis_ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const struct SelvaModify_HierarchyNode *node __unused) {
    return;
}

void Selva_Subscriptions_DeferTriggerEvents(
        struct RedisModuleCtx *redis_ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const struct SelvaModify_HierarchyNode *node __unused,
        enum Selva_SubscriptionTriggerType event_type __unused) {
    return;
}

void SelvaSubscriptions_RefreshByMarker(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        SVector *markers __unused) {
    return;
}

void SelvaSubscriptions_SendDeferredEvents(struct SelvaModify_Hierarchy *hierarchy __unused) {
    return;
}
