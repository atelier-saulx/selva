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
    SELVA_OBJECT_POINTER = 7,
};

struct SelvaObject;
struct SelvaSet;
struct RedisModuleString;

typedef void SelvaObject_Iterator; /* Opaque type. */

#define selvaobject_autofree __attribute__((cleanup(_cleanup_SelvaObject_Destroy)))

struct SelvaObject *SelvaObject_New(void);
void SelvaObject_Clear(struct SelvaObject *obj);
void SelvaObject_Destroy(struct SelvaObject *obj);
void _cleanup_SelvaObject_Destroy(struct SelvaObject **obj);
int SelvaObject_Key2Obj(RedisModuleKey *key, struct SelvaObject **out);
int SelvaObject_DelKeyStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
int SelvaObject_DelKey(struct SelvaObject *obj, const struct RedisModuleString *key_name);
int SelvaObject_ExistsStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
int SelvaObject_Exists(struct SelvaObject *obj, const struct RedisModuleString *key_name);
/**
 * Check if the top-level of the given key exists in obj.
 * The part after the first dot doesn't need to exist.
 */
int SelvaObject_ExistsTopLevel(struct SelvaObject *obj, const struct RedisModuleString *key_name);
int SelvaObject_GetDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double *out);
int SelvaObject_GetDouble(struct SelvaObject *obj, const struct RedisModuleString *key_name, double *out);
int SelvaObject_GetLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long *out);
int SelvaObject_GetLongLong(struct SelvaObject *obj, const struct RedisModuleString *key_name, long long *out);
int SelvaObject_GetStringStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, RedisModuleString **out);
int SelvaObject_GetString(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString **out);
int SelvaObject_SetDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value);
int SelvaObject_SetDouble(struct SelvaObject *obj, const struct RedisModuleString *key_name, double value);
int SelvaObject_SetLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value);
int SelvaObject_SetLongLong(struct SelvaObject *obj, const struct RedisModuleString *key_name, long long value);
int SelvaObject_SetStringStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct RedisModuleString *value);
/**
 * Set a string value.
 * @param key_name is the name of the key ob obj. The argument is used only for lookup and does't need to be retained.
 * @param value is the value; the caller needs to make sure the string is retained.
 */
int SelvaObject_SetString(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString *value);
int SelvaObject_GetObjectStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct SelvaObject **out);
int SelvaObject_GetObject(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct SelvaObject **out);
int SelvaObject_AddDoubleSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, double value);
int SelvaObject_AddLongLongSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, long long value);
int SelvaObject_AddStringSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString *value);
int SelvaObject_AddArray(struct SelvaObject *obj, const struct RedisModuleString *key_name, enum SelvaObjectType subtype, void *p);
int SelvaObject_GetArrayStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType *out_subtype, void **out_p);
int SelvaObject_GetArray(struct SelvaObject *obj, const struct RedisModuleString *key_name, enum SelvaObjectType *out_subtype, void **out_p);
int SelvaObject_SetPointerStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void *p);
int SelvaObject_SetPointer(struct SelvaObject *obj, const struct RedisModuleString *key_name, void *p);
int SelvaObject_GetPointer(struct SelvaObject *obj, const struct RedisModuleString *key_name, void **out_p);
enum SelvaObjectType SelvaObject_GetTypeStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
enum SelvaObjectType SelvaObject_GetType(struct SelvaObject *obj, const struct RedisModuleString *key_name);
int SelvaObject_RemDoubleSet(struct SelvaObject *obj, const RedisModuleString *key_name, double value);
int SelvaObject_RemLongLongSet(struct SelvaObject *obj, const RedisModuleString *key_name, long long value);
int SelvaObject_RemStringSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString *value);
struct SelvaSet *SelvaObject_GetSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
struct SelvaSet *SelvaObject_GetSet(struct SelvaObject *obj, const struct RedisModuleString *key_name);
ssize_t SelvaObject_LenStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
ssize_t SelvaObject_Len(struct SelvaObject *obj, const struct RedisModuleString *key_name);
SelvaObject_Iterator *SelvaObject_ForeachBegin(struct SelvaObject *obj);
const char *SelvaObject_ForeachKey(struct SelvaObject *obj, SelvaObject_Iterator **iterator);
/**
 * Foreach value in object.
 * @param name_out is a direct pointer to the name and it will be rendered invalid if the key is deleted.
 */
const void *SelvaObject_ForeachValue(
        struct SelvaObject *obj,
        SelvaObject_Iterator **iterator,
        const char **name_out,
        enum SelvaObjectType type);
const char *SelvaObject_Type2String(enum SelvaObjectType type, size_t *len);

/*
 * Send a SelvaObject as a Redis reply.
 */
int SelvaObject_ReplyWithObject(RedisModuleCtx *ctx, struct SelvaObject *obj, const RedisModuleString *key_name);

#endif /* SELVA_OBJECT */
