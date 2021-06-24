#pragma once
#ifndef SELVA_NODE
#define SELVA_NODE

#include "selva.h"
#include "redismodule.h"
#include "selva_object.h"

struct RedisModuleKey;
struct RedisModuleCtx;
struct RedisModuleString;

int SelvaNode_Initialize(
        struct RedisModuleCtx *ctx,
        struct RedisModuleKey *key,
        struct RedisModuleString *key_name,
        const Selva_NodeId nodeId);

int SelvaNode_Delete(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *id);

int SelvaNode_ClearFields(struct RedisModuleCtx *ctx, struct SelvaObject *obj);

static int open_node_key(struct RedisModuleCtx *ctx, const Selva_NodeId nodeId, struct RedisModuleKey **key_out, struct SelvaObject **obj_out);

#endif /* SELVA_NODE */
