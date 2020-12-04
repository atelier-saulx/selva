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
    SELVA_OBJECT_SET = 5,
    SELVA_OBJECT_ARRAY = 6,
};

struct SelvaObject;
struct SelvaSet;
struct RedisModuleString;

typedef void SelvaObject_Iterator; /* Opaque type. */

#define selvaobject_autofree __attribute__((cleanup(_cleanup_SelvaObject_Destroy)))

struct SelvaObject *SelvaObject_New(void);
void SelvaObject_Destroy(struct SelvaObject *obj);
void _cleanup_SelvaObject_Destroy(struct SelvaObject **obj);
int SelvaObject_Key2Obj(RedisModuleKey *key, struct SelvaObject **out);
int SelvaObject_DelKey(struct SelvaObject *obj, const struct RedisModuleString *key_name);
int SelvaObject_Exists(struct SelvaObject *obj, const struct RedisModuleString *key_name);
/**
 * Check if the top-level of the given key exists in obj.
 * The part after the first dot doesn't need to exist.
 */
int SelvaObject_ExistsTopLevel(struct SelvaObject *obj, const struct RedisModuleString *key_name);
int SelvaObject_GetDouble(struct SelvaObject *obj, const struct RedisModuleString *key_name, double *out);
int SelvaObject_GetLongLong(struct SelvaObject *obj, const struct RedisModuleString *key_name, long long *out);
int SelvaObject_GetStr(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString **out);
int SelvaObject_SetDouble(struct SelvaObject *obj, const struct RedisModuleString *key_name, double value);
int SelvaObject_SetLongLong(struct SelvaObject *obj, const struct RedisModuleString *key_name, long long value);
int SelvaObject_SetStr(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString *value);
int SelvaObject_GetObject(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct SelvaObject **out);
int SelvaObject_AddSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString *value);
int SelvaObject_AddArray(struct SelvaObject *obj, const struct RedisModuleString *key_name, enum SelvaObjectType subtype, void *p);
int SelvaObject_GetArray(struct SelvaObject *obj, const struct RedisModuleString *key_name, enum SelvaObjectType *out_subtype, void **out_p);
enum SelvaObjectType SelvaObject_GetTypeStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
enum SelvaObjectType SelvaObject_GetType(struct SelvaObject *obj, struct RedisModuleString *key_name);
int SelvaObject_RemSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString *value);
struct SelvaSet *SelvaObject_GetSet(struct SelvaObject *obj, const struct RedisModuleString *key_name);
ssize_t SelvaObject_Len(struct SelvaObject *obj, struct RedisModuleString *key_name);
SelvaObject_Iterator *SelvaObject_ForeachBegin(struct SelvaObject *obj);
const char *SelvaObject_ForeachKey(struct SelvaObject *obj, SelvaObject_Iterator **iterator);
const void *SelvaObject_ForeachValue(struct SelvaObject *obj, SelvaObject_Iterator **iterator, enum SelvaObjectType type);

/*
 * Send a SelvaObject as a Redis reply.
 */
int SelvaObject_ReplyWithObject(RedisModuleCtx *ctx, struct SelvaObject *obj, RedisModuleString *key_name);

#endif /* SELVA_OBJECT */
