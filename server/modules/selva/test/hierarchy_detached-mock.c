#include "redismodule.h"
#include "errors.h"
#include "hierarchy.h"
#include "../module/hierarchy/hierarchy_detached.h"

void *SelvaHierarchyDetached_Store(
        const Selva_NodeId node_id,
        struct compressed_rms *compressed,
        enum SelvaHierarchyDetachedType type) {
    return NULL;
}

int SelvaHierarchyDetached_Get(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        struct compressed_rms **compressed,
        enum SelvaHierarchyDetachedType *type) {
    return SELVA_ENOENT;
}

void SelvaHierarchyDetached_RemoveNode(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id) {
}

int SelvaHierarchyDetached_AddNode(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        void *tag_compressed) {
    return 0;
}
