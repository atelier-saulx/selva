#pragma once
#ifndef SELVA_RESOLVE
#define SELVA_RESOLVE

#include <stddef.h>
#include "selva.h"

struct RedisModuleCtx;
struct RedisModuleKey;
struct RedisModuleString;
struct SelvaModify_Hierarchy;

int SelvaResolve_NodeId(
        struct RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        RedisModuleString **ids,
        size_t nr_ids,
        Selva_NodeId node_id);

#endif /* SELVA_RESOLVE */
