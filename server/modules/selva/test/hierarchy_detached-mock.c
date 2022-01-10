#include "redismodule.h"
#include "errors.h"
#include "hierarchy.h"
#include "../module/hierarchy/hierarchy_detached.h"

int SelvaHierarchyDetached_Get(struct SelvaHierarchy *hierarchy, const Selva_NodeId node_id, struct compressed_rms **compressed) {
    return SELVA_ENOENT;
}

void SelvaHierarchyDetached_RemoveNode(SelvaHierarchy *hierarchy, const Selva_NodeId node_id) {
}

int SelvaHierarchyDetached_AddNode(struct SelvaHierarchy *hierarchy, const Selva_NodeId node_id, struct compressed_rms *compressed) {
    return 0;
}
