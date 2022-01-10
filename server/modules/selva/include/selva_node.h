#pragma once
#ifndef SELVA_NODE
#define SELVA_NODE

#include "selva.h"
#include "redismodule.h"
#include "selva_object.h"

struct RedisModuleKey;
struct RedisModuleCtx;
struct RedisModuleString;

int SelvaNode_ClearFields(struct SelvaObject *obj);

#endif /* SELVA_NODE */
