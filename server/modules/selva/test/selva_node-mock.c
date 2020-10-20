#include "cdefs.h"
#include "selva_node.h"

int SelvaNode_Delete(
        struct RedisModuleCtx *ctx __unused,
        struct RedisModuleString *id __unused) {
    return 0;
}

int SelvaNode_ExistField(
        struct RedisModuleCtx *ctx __unused,
        struct RedisModuleKey *node_key __unused,
        const struct RedisModuleString *field __unused) {
    return 0;
}

int SelvaNode_GetField(
        struct RedisModuleCtx *ctx __unused,
        struct RedisModuleKey *node_key __unused,
        const struct RedisModuleString *field __unused,
        struct RedisModuleString **out __unused) {
    return 0;
}

int SelvaNode_SetField(
        struct RedisModuleCtx *ctx __unused,
        struct RedisModuleKey *node_key __unused,
        struct RedisModuleString *field __unused,
        struct RedisModuleString *value __unused) {
    return 0;
}

int SelvaNode_DelField(
        struct RedisModuleCtx *ctx __unused,
        struct RedisModuleKey *node_key __unused,
        struct RedisModuleString *field __unused) {
    return 0;
}
