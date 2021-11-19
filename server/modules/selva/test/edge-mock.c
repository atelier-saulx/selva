#include <stddef.h>
#include <string.h>
#include "hierarchy.h"
#include "edge.h"
#include "redismodule.h"

struct EdgeField *Edge_GetField(const struct SelvaHierarchyNode *node, const char *key_name_str, size_t key_name_len) {
    return NULL;
}

int Edge_Refcount(struct SelvaHierarchyNode *node) {
    return 0;
}

void Edge_InitEdgeFieldConstraints(struct EdgeFieldConstraints *data) {
    memset(data, 0, sizeof(*data));
}

void Edge_DeinitEdgeFieldConstraints(struct EdgeFieldConstraints *data) {
    memset(data, 0, sizeof(*data));
}

int Edge_Usage(const struct SelvaHierarchyNode *node) {
    return 0;
}

int Edge_RdbLoad(struct RedisModuleIO *io, int encver, struct SelvaHierarchy *hierarchy, struct SelvaHierarchyNode *node) {
    RedisModule_LoadUnsigned(io);
    return 0;
}

void Edge_RdbSave(struct RedisModuleIO *io, struct SelvaHierarchyNode *node) {
    RedisModule_SaveUnsigned(io, 0);
}

int EdgeConstraint_RdbLoad(struct RedisModuleIO *io, int encver, struct EdgeFieldConstraints *data) {
    return 0;
}

void EdgeConstraint_RdbSave(struct RedisModuleIO *io, struct EdgeFieldConstraints *data) {
    return;
}
