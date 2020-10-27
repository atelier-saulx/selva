#pragma once
#ifndef SELVA_OBJECT
#define SELVA_OBJECT

/*
 * Object key types.
 * DO NOT REORDER the numbers as they are used for in the RDB storage format.
 */
enum SelvaObjectType {
    SELVA_OBJECT_NULL = 0,
    SELVA_OBJECT_DOUBLE = 1,
    SELVA_OBJECT_LONGLONG = 2,
    SELVA_OBJECT_STRING = 3,
    SELVA_OBJECT_OBJECT = 4,
    SELVA_OBJECT_SET_REF = 5,
};

struct SelvaObject;
struct RedisModuleString;

int SelvaObject_Key2Obj(RedisModuleKey *key, struct SelvaObject **out);
int SelvaObject_DelKey(struct SelvaObject *obj, const struct RedisModuleString *key_name);
int SelvaObject_Exists(struct SelvaObject *obj, const struct RedisModuleString *key_name);
int SelvaObject_GetDouble(struct SelvaObject *obj, const struct RedisModuleString *key_name, double *out);
int SelvaObject_GetLongLong(struct SelvaObject *obj, const struct RedisModuleString *key_name, long long *out);
int SelvaObject_GetStr(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString **out);
int SelvaObject_SetDouble(struct SelvaObject *obj, const struct RedisModuleString *key_name, double value);
int SelvaObject_SetLongLong(struct SelvaObject *obj, const struct RedisModuleString *key_name, double value);
int SelvaObject_SetStr(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString *value);
int SelvaObject_SetSetRef(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString *value);
enum SelvaObjectType SelvaObject_GetType(struct SelvaObject *obj, const char *key_name, size_t key_name_len);

/*
 * Send a SelvaObject as a Redis reply.
 */
int SelvaObject_ReplyWithObject(RedisModuleCtx *ctx, struct SelvaObject *obj, RedisModuleString *key_name);

#endif /* SELVA_OBJECT */
