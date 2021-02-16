#include "cdefs.h"
#include "selva.h"
#include "subscriptions.h"

struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyMetadata;

int SelvaSubscriptions_InitDeferredEvents(struct SelvaModify_Hierarchy *hierarchy __unused) {
    return 0;
}

int SelvaSubscriptions_InitMarkersStruct(struct Selva_SubscriptionMarkers *markers __unused) {
    return 0;
}

void SelvaSubscriptions_DestroyAll(struct SelvaModify_Hierarchy *hierarchy __unused) {
    return;
}
void SelvaSubscriptions_ClearAllMarkers(
        struct SelvaModify_Hierarchy *hierarchy __unused,
        Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *metadata __unused) {
    return;
}

void SelvaSubscriptions_InheritParent(
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *node_metadata __unused,
        size_t node_nr_children __unused,
        const Selva_NodeId parent_id __unused,
        struct SelvaModify_HierarchyMetadata *parent_metadata __unused) {
    return;
}

void SelvaSubscriptions_InheritChild(
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *node_metadata __unused,
        size_t node_nr_parents __unused,
        const Selva_NodeId child_id __unused,
        struct SelvaModify_HierarchyMetadata *child_metadata __unused) {
    return;
}

void SelvaSubscriptions_DeferMissingAccessorEvents(struct SelvaModify_Hierarchy *hierarchy, const char *id_str, size_t id_len) {
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
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        const struct SelvaModify_HierarchyMetadata *metadata __unused) {
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
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        enum Selva_SubscriptionTriggerType event_type) {
    return;
}

void SelvaSubscriptions_RefreshByMarker(
        struct SelvaModify_Hierarchy *hierarchy __unused,
        SVector *markers __unused) {
    return;
}

void SelvaSubscriptions_SendDeferredEvents(struct SelvaModify_Hierarchy *hierarchy __unused) {
    return;
}
