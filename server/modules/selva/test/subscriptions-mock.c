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

void SelvaSubscriptions_DestroyAll(struct RedisModuleCtx *ctx __unused, struct SelvaHierarchy *hierarchy) {
    struct SelvaSubscriptions_DeferredEvents *def = &hierarchy->subs.deferred_events;

    SVector_Destroy(&def->updates);
    SVector_Destroy(&def->triggers);
    SelvaObject_Destroy(hierarchy->subs.missing);
}
void SelvaSubscriptions_ClearAllMarkers(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        struct SelvaHierarchyNode *node __unused) {
    return;
}

void SelvaSubscriptions_InheritParent(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        struct SelvaHierarchyMetadata *node_metadata __unused,
        size_t node_nr_children __unused,
        struct SelvaHierarchyNode *parent __unused) {
    return;
}

void SelvaSubscriptions_InheritChild(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        const Selva_NodeId node_id __unused,
        struct SelvaHierarchyMetadata *node_metadata __unused,
        size_t node_nr_parents __unused,
        struct SelvaHierarchyNode *child __unused) {
    return;
}

void SelvaSubscriptions_InheritEdge(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        struct SelvaHierarchyNode *src_node __unused,
        struct SelvaHierarchyNode *dst_node __unused,
        const char *field_str __unused,
        size_t field_len __unused) {
    return;
}

int SelvaSubscriptions_DeleteMarker(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        const Selva_SubscriptionId sub_id __unused,
        Selva_SubscriptionMarkerId marker_id __unused) {
    return 0;
}
int SelvaSubscriptions_DeleteMarkerByPtr(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        struct Selva_SubscriptionMarker *marker __unused) {
    return 0;
}

void SelvaSubscriptions_DeferMissingAccessorEvents(struct SelvaHierarchy *hierarchy __unused, const char *id_str __unused, size_t id_len __unused) {
    return;
}

void SelvaSubscriptions_DeferFieldChangeEvents(
        struct RedisModuleCtx *redis_ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        const struct SelvaHierarchyNode *node __unused,
        const char *field_str __unused,
        size_t field_len __unused) {
    return;
}

void SelvaSubscriptions_DeferHierarchyEvents(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        const struct SelvaHierarchyNode *node __unused) {
    return;
}

void SelvaSubscriptions_DeferHierarchyDeletionEvents(
        struct RedisModuleCtx *redis_ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        const struct SelvaHierarchyNode *node __unused) {
    return;
}

void Selva_Subscriptions_DeferTriggerEvents(
        struct RedisModuleCtx *redis_ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        const struct SelvaHierarchyNode *node __unused,
        enum Selva_SubscriptionTriggerType event_type __unused) {
    return;
}

void SelvaSubscriptions_RefreshByMarker(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        SVector *markers __unused) {
    return;
}

void SelvaSubscriptions_SendDeferredEvents(struct SelvaHierarchy *hierarchy __unused) {
    return;
}
