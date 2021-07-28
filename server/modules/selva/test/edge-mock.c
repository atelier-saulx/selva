#include <stddef.h>
#include <string.h>
#include "hierarchy.h"
#include "edge.h"
#include "redismodule.h"

struct EdgeField *Edge_GetField(struct SelvaModify_HierarchyNode *node __unused, const char *key_name_str __unused, size_t key_name_len __unused) {
    return NULL;
}

int Edge_Refcount(struct SelvaModify_HierarchyNode *node __unused) {
    return 0;
}

void Edge_InitEdgeFieldConstraints(struct EdgeFieldConstraints *data) {
    memset(data, 0, sizeof(*data));
}

int Edge_RdbLoad(struct RedisModuleIO *io, int encver __unused, struct SelvaModify_Hierarchy *hierarchy __unused, struct SelvaModify_HierarchyNode *node __unused) {
    RedisModule_LoadUnsigned(io);
    return 0;
}

void Edge_RdbSave(struct RedisModuleIO *io, struct SelvaModify_HierarchyNode *node __unused) {
    RedisModule_SaveUnsigned(io, 0);
}

int EdgeConstraint_RdbLoad(struct RedisModuleIO *io __unused, int encver __unused, struct EdgeFieldConstraints *data __unused) {
    return 0;
}

void EdgeConstraint_RdbSave(struct RedisModuleIO *io __unused, struct EdgeFieldConstraints *data __unused) {
    return;
}
