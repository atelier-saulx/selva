#include <stddef.h>
#include "hierarchy.h"
#include "edge.h"
#include "redismodule.h"

struct EdgeField *Edge_GetField(struct SelvaModify_HierarchyNode *node, const char *key_name_str, size_t key_name_len) {
    return NULL;
}

int Edge_RdbLoad(struct RedisModuleIO *io, int encver __unused, struct SelvaModify_Hierarchy *hierarchy __unused, struct SelvaModify_HierarchyNode *node __unused) {
    RedisModule_LoadUnsigned(io);
    return 0;
}

void Edge_RdbSave(struct RedisModuleIO *io, struct SelvaModify_HierarchyNode *node __unused) {
    RedisModule_SaveUnsigned(io, 0);
}
