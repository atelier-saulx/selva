#include "cdefs.h"
#include "selva.h"
#include "errors.h"
#include "hierarchy.h"
#include "subscriptions.h"
#include "selva_object.h"

struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyMetadata;

int SelvaSubscriptions_InitDeferredEvents(struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    return !SVector_Init(&def->updates, 2, NULL) ||
           !SVector_Init(&def->triggers, 3, NULL)
           ? SELVA_SUBSCRIPTIONS_ENOMEM : 0;
}

int SelvaSubscriptions_InitMarkersStruct(struct Selva_SubscriptionMarkers *markers) {
    if (!SVector_Init(&markers->vec, 0, NULL)) {
        return SELVA_SUBSCRIPTIONS_ENOMEM;
    }

    markers->flags_filter = 0;

    return 0;
}

void SelvaSubscriptions_DestroyAll(struct SelvaModify_Hierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    SVector_Destroy(&def->updates);
    SVector_Destroy(&def->triggers);
    SelvaObject_Destroy(hierarchy->subs.missing);
}
void SelvaSubscriptions_ClearAllMarkers(
        struct SelvaModify_Hierarchy *hierarchy __unused,
        struct SelvaModify_HierarchyNode *node) {
    return;
}

void SelvaSubscriptions_InheritParent(
        struct RedisModuleCtx *redis_ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *node_metadata __unused,
        size_t node_nr_children __unused,
        const Selva_NodeId parent_id __unused,
        struct SelvaModify_HierarchyMetadata *parent_metadata __unused) {
    return;
}

void SelvaSubscriptions_InheritChild(
        struct RedisModuleCtx *redis_ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *node_metadata __unused,
        size_t node_nr_parents __unused,
        const Selva_NodeId child_id __unused,
        struct SelvaModify_HierarchyMetadata *child_metadata __unused) {
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

void SelvaSubscriptions_DeferMissingAccessorEvents(struct SelvaModify_Hierarchy *hierarchy __unused, const char *id_str __unused, size_t id_len __unused) {
    return;
}

void SelvaSubscriptions_DeferFieldChangeEvents(
        struct RedisModuleCtx *redis_ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        const struct SelvaModify_HierarchyMetadata *metadata __unused,
        const char *field __unused) {
    return;
}

void SelvaSubscriptions_DeferHierarchyEvents(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        const struct SelvaModify_HierarchyMetadata *metadata __unused,
        int ignore_filter) {
    return;
}

void SelvaSubscriptions_DeferHierarchyDeletionEvents(
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        const struct SelvaModify_HierarchyMetadata *metadata __unused) {
    return;
}

void Selva_Subscriptions_DeferTriggerEvents(
        struct RedisModuleCtx *redis_ctx __unused,
        struct SelvaModify_Hierarchy *hierarchy __unused,
        Selva_NodeId node_id __unused,
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
