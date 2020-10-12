#pragma once
#ifndef SELVA_SET
#define SELVA_SET

#include "selva.h"

struct RedisModuleCtx;
struct RedisModuleKey;
struct RedisModuleString;

struct RedisModuleKey *SelvaSet_Open(struct RedisModuleCtx *ctx, const char *id_str, size_t id_len, const char *field_str);
int SelvaSet_Remove(struct RedisModuleKey *set_key, struct RedisModuleKey *alias_key);

#endif /* SELVA_SET */
