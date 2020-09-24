#include "cdefs.h"
#include "selva.h"
#include "subscriptions.h"

struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyMetadata;

int SelvaSubscriptions_InitDeferredEvents(struct SelvaModify_Hierarchy *hierarchy) {
    return 0;
}

int SelvaSubscriptions_InitMarkersStruct(struct Selva_SubscriptionMarkers *markers) {
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
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *node_metadata,
        size_t node_nr_children,
        const Selva_NodeId parent_id,
        struct SelvaModify_HierarchyMetadata *parent_metadata) {
    return;
}

void SelvaSubscriptions_InheritChild(
        struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id __unused,
        struct SelvaModify_HierarchyMetadata *node_metadata,
        size_t node_nr_parents,
        const Selva_NodeId child_id,
        struct SelvaModify_HierarchyMetadata *child_metadata) {
    return;
}

void SelvaSubscriptions_DeferFieldChangeEvents(
        struct SelvaModify_Hierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        const struct SelvaModify_HierarchyMetadata *metadata __unused,
        const char *field __unused) {
    return;
}

void SelvaSubscriptions_DeferHierarchyEvents(struct SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId node_id,
        const struct SelvaModify_HierarchyMetadata *metadata) {
    return;
}

void SelvaSubscriptions_RefreshByMarker(struct SelvaModify_Hierarchy *hierarchy, SVector *markers) {
    return;
}

void SelvaSubscriptions_SendDeferredEvents(struct SelvaModify_Hierarchy *hierarchy) {
    return;
}
