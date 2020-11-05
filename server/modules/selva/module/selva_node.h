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

int SelvaNode_Delete(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *id);

#endif /* SELVA_NODE */
