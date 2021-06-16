#pragma once
#ifndef SELVA_OBJECT
#define SELVA_OBJECT

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

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

struct RedisModuleCtx;
struct RedisModuleIO;
struct RedisModuleKey;
struct RedisModuleString;
struct SVector;
struct SelvaObject;
struct SelvaSet;

typedef uint32_t SelvaObjectMeta_t;
typedef void SelvaObject_Iterator; /* Opaque type. */
typedef void *(*SelvaObject_PtrLoad)(struct RedisModuleIO *io, int encver, void *data);
typedef void (*SelvaObject_PtrSave)(struct RedisModuleIO *io, void *value, void *data);

struct SelvaObjectPointerOpts {
    /**
     * An unique id for serializing the pointer type.
     * The value 0 is reserved for NOP.
     */
    unsigned ptr_type_id;

    /**
     * Send the pointer value as a reply to the client.
     */
    void (*ptr_reply)(struct RedisModuleCtx *ctx, void *p);

    /**
     * Free a SELVA_OBJECT_POINTER value.
     */
    void (*ptr_free)(void *p);

    /**
     * Get the length or size of a SELVA_OBJECT_POINTER value.
     * The unit of the size is undefined but typically it should be either a
     * count of items or the byte size of the value.
     */
    size_t (*ptr_len)(void *p);

    SelvaObject_PtrLoad ptr_load;
    SelvaObject_PtrSave ptr_save;
};

/*
 * Pointer types.
 * These types are needed for the serialization of opaque pointer types.
 */
#define SELVA_OBJECT_POINTER_EDGE 1
#define SELVA_OBJECT_POINTER_LANG 2

/**
 * Register SELVA_OBJECT_POINTER options statically for RDB loading.
 */
#define SELVA_OBJECT_POINTER_OPTS(opts) \
    DATA_SET(selva_objpop, opts)

#define selvaobject_autofree __attribute__((cleanup(_cleanup_SelvaObject_Destroy)))

struct SelvaObject *SelvaObject_New(void);
void SelvaObject_Clear(struct SelvaObject *obj);
void SelvaObject_Destroy(struct SelvaObject *obj);
void _cleanup_SelvaObject_Destroy(struct SelvaObject **obj);
int SelvaObject_Key2Obj(struct RedisModuleKey *key, struct SelvaObject **out);

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
int SelvaObject_GetStringStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct RedisModuleString **out);
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

int SelvaObject_IncrementDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double default_value, double incr);
int SelvaObject_IncrementDouble(struct SelvaObject *obj, const struct RedisModuleString *key_name, double default_value, double incr);
int SelvaObject_IncrementLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long default_value, long long incr);
int SelvaObject_IncrementLongLong(struct SelvaObject *obj, const struct RedisModuleString *key_name, long long default_value, long long incr);

int SelvaObject_GetObjectStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct SelvaObject **out);
int SelvaObject_GetObject(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct SelvaObject **out);

int SelvaObject_AddDoubleSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, double value);
int SelvaObject_AddLongLongSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, long long value);
int SelvaObject_AddStringSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString *value);
int SelvaObject_AddArrayStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, void *p);
int SelvaObject_AddArray(struct SelvaObject *obj, const struct RedisModuleString *key_name, enum SelvaObjectType subtype, void *p);
int SelvaObject_InsertArrayStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, void *p);
int SelvaObject_InsertArray(struct SelvaObject *obj, const struct RedisModuleString *key_name, enum SelvaObjectType subtype, void *p);
int SelvaObject_InsertArray(struct SelvaObject *obj, const struct RedisModuleString *key_name, enum SelvaObjectType subtype, void *p);
int SelvaObject_AssignArrayIndexStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, size_t idx, void *p);
int SelvaObject_GetArrayStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType *out_subtype, struct SVector **out_p);
int SelvaObject_GetArray(struct SelvaObject *obj, const struct RedisModuleString *key_name, enum SelvaObjectType *out_subtype, struct SVector **out_p);
int SelvaObject_RemoveArrayIndex(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, size_t idx);
int SelvaObject_GetArrayIndexAsRmsStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, size_t idx, struct RedisModuleString **out);
int SelvaObject_GetArrayIndexAsSelvaObject(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, size_t idx, struct SelvaObject **out);
int SelvaObject_GetArrayIndexAsLongLong(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, size_t idx, long long *out);
int SelvaObject_GetArrayIndexAsDouble(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, size_t idx, double *out);
size_t SelvaObject_GetArrayLen(struct SelvaObject *obj, const struct RedisModuleString *key_name);
size_t SelvaObject_GetArrayLenStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);

/**
 * Set a pointer value.
 * @param p is the pointer value and it must be non-NULL.
 * @param opts is an optional pointer to SELVA_OBJECT_POINTER ops that can define
 *             how to free the data pointed by the pointer or how to serialize it.
 */
int SelvaObject_SetPointerStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void *p, const struct SelvaObjectPointerOpts *opts);
int SelvaObject_SetPointer(struct SelvaObject *obj, const struct RedisModuleString *key_name, void *p, const struct SelvaObjectPointerOpts *opts);
int SelvaObject_GetPointerStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void **out_p);
int SelvaObject_GetPointer(struct SelvaObject *obj, const struct RedisModuleString *key_name, void **out_p);
int SelvaObject_GetPointerPartialMatchStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void **out_p);

enum SelvaObjectType SelvaObject_GetTypeStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
enum SelvaObjectType SelvaObject_GetType(struct SelvaObject *obj, const struct RedisModuleString *key_name);

int SelvaObject_RemDoubleSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value);
int SelvaObject_RemDoubleSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, double value);
int SelvaObject_RemLongLongSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value);
int SelvaObject_RemLongLongSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, long long value);
int SelvaObject_RemStringSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct RedisModuleString *value);
int SelvaObject_RemStringSet(struct SelvaObject *obj, const struct RedisModuleString *key_name, struct RedisModuleString *value);
struct SelvaSet *SelvaObject_GetSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
struct SelvaSet *SelvaObject_GetSet(struct SelvaObject *obj, const struct RedisModuleString *key_name);

ssize_t SelvaObject_LenStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
ssize_t SelvaObject_Len(struct SelvaObject *obj, const struct RedisModuleString *key_name);

int SelvaObject_GetUserMetaStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, SelvaObjectMeta_t *meta);
int SelvaObject_GetUserMeta(struct SelvaObject *obj, const struct RedisModuleString *key_name, SelvaObjectMeta_t *meta);
int SelvaObject_SetUserMetaStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, SelvaObjectMeta_t meta, SelvaObjectMeta_t *old_meta);
int SelvaObject_SetUserMeta(struct SelvaObject *obj, const struct RedisModuleString *key_name, SelvaObjectMeta_t meta, SelvaObjectMeta_t *old_meta);
SelvaObject_Iterator *SelvaObject_ForeachBegin(struct SelvaObject *obj);
const char *SelvaObject_ForeachKey(struct SelvaObject *obj, SelvaObject_Iterator **iterator);
int SelvaObject_GetWithWildcardStr(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *lang,
        struct SelvaObject *obj,
        const char *okey_str,
        size_t okey_len,
        long *resp_count,
        int resp_path_start_idx,
        unsigned int flags);
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
int SelvaObject_ReplyWithObject(struct RedisModuleCtx *ctx, struct RedisModuleString *lang, struct SelvaObject *obj, const struct RedisModuleString *key_name);

struct SelvaObject *SelvaObjectTypeRDBLoad(struct RedisModuleIO *io, int encver, void *ptr_load_data);
void SelvaObjectTypeRDBSave(struct RedisModuleIO *io, struct SelvaObject *obj, void *ptr_save_data);

#endif /* SELVA_OBJECT */
