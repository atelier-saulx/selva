#pragma once
#ifndef SELVA_ALIAS_H
#define SELVA_ALIAS_H

struct RedisModuleCtx;
struct RedisModuleKey;
struct RedisModuleString;
struct SelvaSet;

struct RedisModuleKey *open_aliases_key(struct RedisModuleCtx *ctx);
int delete_aliases(struct RedisModuleKey *aliases_key, struct SelvaSet *set);
void update_alias(
        struct RedisModuleCtx *ctx,
        struct RedisModuleKey *alias_key,
        struct RedisModuleString *id,
        struct RedisModuleString *ref);

#endif /* SELVA_ALIAS_H */
