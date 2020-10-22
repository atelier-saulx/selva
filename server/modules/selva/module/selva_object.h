#pragma once
#ifndef SELVA_OBJECT
#define SELVA_OBJECT

struct SelvaObject;
struct RedisModuleString;

int SelvaObject_Key2Obj(RedisModuleKey *key, struct SelvaObject **out);
int SelvaObject_DelKey(struct SelvaObject *obj, const RedisModuleString *key_name);
int SelvaObject_Exists(struct SelvaObject *obj, const RedisModuleString *key_name);
int SelvaObject_GetDouble(struct SelvaObject *obj, const RedisModuleString *key_name, double *out);
int SelvaObject_GetLongLong(struct SelvaObject *obj, const RedisModuleString *key_name, long long *out);
int SelvaObject_GetStr(struct SelvaObject *obj, const RedisModuleString *key_name, RedisModuleString **out);
int SelvaObject_SetDouble(struct SelvaObject *obj, const RedisModuleString *key_name, double value);
int SelvaObject_SetLongLong(struct SelvaObject *obj, const RedisModuleString *key_name, double value);
int SelvaObject_SetStr(struct SelvaObject *obj, const RedisModuleString *key_name, RedisModuleString *value);

/*
 * Send a SelvaObject as a Redis reply.
 */
int SelvaObject_ReplyWithObject(RedisModuleCtx *ctx, struct SelvaObject *obj, RedisModuleString *key_name);

#endif /* SELVA_OBJECT */
