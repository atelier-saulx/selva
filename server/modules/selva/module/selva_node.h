#pragma once
#ifndef SELVA_NODE
#define SELVA_NODE

#include "selva.h"

struct RedisModuleKey;
struct RedisModuleCtx;
struct SelvaModify_Hierarchy;
struct RedisModuleString;

#define SELVA_NODE_OPEN_CREATE_FLAG     0x01 /*!< Create the node if it didn't exist. */
#define SELVA_NODE_OPEN_NO_ROOT_FLAG    0x02 /*!< Don't mark the root node as a parent when creating. */
#define SELVA_NODE_OPEN_WRFLD_FLAG      0x08 /*!< Open the node for writing fields. */

/**
 * Open a node key.
 * When calling this from hierarchy the hierarchy pointer can be left NULL and
 * SELVA_NODE_OPEN_CREATE_FLAG nor SELVA_NODE_OPEN_NO_ROOT_FLAG has no effect.
 */
struct RedisModuleKey *SelvaNode_Open(
        struct RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        struct RedisModuleString *id,
        const Selva_NodeId nodeId,
        unsigned flags);

int SelvaNode_Delete(RedisModuleCtx *ctx, RedisModuleString *id);

int SelvaNode_ExistField(
        struct RedisModuleCtx *ctx,
        struct RedisModuleKey *node_key,
        struct RedisModuleString *field);

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
