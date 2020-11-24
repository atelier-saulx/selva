#pragma once
#ifndef SELVA_NODE
#define SELVA_NODE

#include "selva.h"

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

#endif /* SELVA_NODE */
