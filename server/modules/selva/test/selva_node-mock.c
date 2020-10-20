#include "selva_node.h"

int SelvaNode_Delete(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *id) {
    return 0;
}

int SelvaNode_ExistField(
        struct RedisModuleCtx *ctx,
        struct RedisModuleKey *node_key,
        const struct RedisModuleString *field) {
    return 0;
}

int SelvaNode_GetField(
        struct RedisModuleCtx *ctx,
        struct RedisModuleKey *node_key,
        const struct RedisModuleString *field,
        struct RedisModuleString **out) {
    return 0;
}

int SelvaNode_SetField(
        struct RedisModuleCtx *ctx,
        struct RedisModuleKey *node_key,
        struct RedisModuleString *field,
        struct RedisModuleString *value) {
    return 0;
}

int SelvaNode_DelField(
        struct RedisModuleCtx *ctx,
        struct RedisModuleKey *node_key,
        struct RedisModuleString *field) {
    return 0;
}
