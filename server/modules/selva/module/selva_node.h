#pragma once
#ifndef SELVA_NODE
#define SELVA_NODE

#include "selva.h"

struct RedisModuleKey;
struct RedisModuleCtx;
struct SelvaModify_Hierarchy;
struct RedisModuleString;

struct RedisModuleKey *SelvaNode_Open(
        struct RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        struct RedisModuleString *id,
        Selva_NodeId nodeId,
        int no_root);

int SelvaNode_GetField(
        struct RedisModuleCtx *ctx,
        struct RedisModuleKey *node_key,
        struct RedisModuleString *field,
        struct RedisModuleString **out);

int SelvaNode_SetField(
        struct RedisModuleCtx *ctx,
        struct RedisModuleKey *node_key,
        struct RedisModuleString *field,
        struct RedisModuleString *value);

int SelvaNode_DelField(
        struct RedisModuleCtx *ctx,
        struct RedisModuleKey *node_key,
        struct RedisModuleString *field);

#endif /* SELVA_NODE */
