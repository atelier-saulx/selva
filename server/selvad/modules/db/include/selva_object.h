/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef SELVA_OBJECT
#define SELVA_OBJECT

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "selva_set.h"
#include "selva_lang.h"

/*
 * Object key types.
 * DO NOT REORDER the numbers as they are used for in the serialization format.
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
    SELVA_OBJECT_HLL = 8,
} __packed;

enum SelvaObjectReplyFlags {
    SELVA_OBJECT_REPLY_SPLICE_FLAG = 0x01, /*!< Set if the path should be spliced to start from the first wildcard. */
    SELVA_OBJECT_REPLY_ANY_OBJ_FLAG = 0x02, /*!< Send any object as a wildcard reply from SelvaObject_GetPointerPartialMatchStr(). */
};

/**
 * Size of struct SelvaObject.
 * This must match with the actual size of selva_object.c won't compile.
 */
#define SELVA_OBJECT_BSIZE 304

/**
 * Define a for holding a SelvaObject.
 */
#define STATIC_SELVA_OBJECT(name) _Alignas(void *) char name[SELVA_OBJECT_BSIZE]

#define GET_STATIC_SELVA_OBJECT(container) \
    ((struct SelvaObject *)((container)->_obj_data))

struct selva_io;
struct SVector;
struct SelvaObject;
struct SelvaSet;
struct selva_server_response_out;
struct selva_string;

typedef uint32_t SelvaObjectMeta_t; /*!< SelvaObject key metadata. */
typedef void SelvaObject_Iterator; /* Opaque type. */
typedef void *(*SelvaObject_PtrLoad)(struct selva_io *io, int encver, void *data);
typedef void (*SelvaObject_PtrSave)(struct selva_io *io, void *value, void *data);

struct SelvaObjectPointerOpts {
    /**
     * An unique type id for serializing the pointer type data.
     */
    enum {
        SELVA_OBJECT_POINTER_NOP = 0, /*!< No operation. No serialization will happen. */
        SELVA_OBJECT_POINTER_EDGE = 1,
        SELVA_OBJECT_POINTER_EDGE_CONSTRAINTS = 2,
        SELVA_OBJECT_POINTER_LANG = 3,
    } ptr_type_id;

    /**
     * Send the pointer value as a reply to the client.
     */
    void (*ptr_reply)(struct selva_server_response_out *resp, void *p);

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

    /**
     * Deserializer for the pointer value.
     */
    SelvaObject_PtrLoad ptr_load;
    /**
     * Serializer for the pointer value.
     */
    SelvaObject_PtrSave ptr_save;
};

/**
 * Register SELVA_OBJECT_POINTER options statically for deserialization.
 */
#define SELVA_OBJECT_POINTER_OPTS(opts) \
    DATA_SET(selva_objpop, opts)

/**
 * Auto free a SelvaObject when code execution exits a block scope.
 */
#define selvaobject_autofree __attribute__((cleanup(_cleanup_SelvaObject_Destroy)))

/**
 * Any value.
 */
struct SelvaObjectAny {
    enum SelvaObjectType type; /*!< Type of the value. */
    enum SelvaObjectType subtype; /*!< Subtype of the value. Arrays use this. */
    SelvaObjectMeta_t user_meta; /*!< User defined metadata. */
    char str_lang[LANG_MAX + 1]; /*!< Language of str if applicable. */
    union {
        double d; /*!< SELVA_OBJECT_DOUBLE */
        long long ll; /*!< SELVA_OBJECT_LONGLONG and SELVA_OBJECT_HLL */
        struct selva_string *str; /* SELVA_OBJECT_STRING */
        struct SelvaObject *obj; /* SELVA_OBJECT_OBJECT */
        struct SelvaSet *set; /*!< SELVA_OBJECT_SET */
        struct SVector *array; /*!< SELVA_OBJECT_ARRAY */
        void *p; /* SELVA_OBJECT_POINTER */
    };
};

/**
 * Create a new SelvaObject.
 * @return Returns a pointer to the newly created object;
 *         In case of OOM a NULL pointer is returned.
 */
struct SelvaObject *SelvaObject_New(void) __attribute__((returns_nonnull, warn_unused_result));
/**
 * Initialize a prealloced buffer as a SelvaObject.
 * The given buffer must be aligned the same way as the struct SelvaObject is
 * aligned.
 */
struct SelvaObject *SelvaObject_Init(char buf[SELVA_OBJECT_BSIZE]) __attribute__((returns_nonnull));
/**
 * Clear all keys in the object, except those listed in exclude.
 */
void SelvaObject_Clear(struct SelvaObject *obj, const char * const exclude[]);
/**
 * Destroy a SelvaObject and free all memory.
 * If the object contains arrays of pointers, the elements pointed won't be
 * freed.
 * If the object contains SELVA_OBJECT_POINTER fields the pointed objects
 * are freed if SelvaObjectPointerOpts is set and ptr_free is set in the ops.
 */
void SelvaObject_Destroy(struct SelvaObject *obj);
void _cleanup_SelvaObject_Destroy(struct SelvaObject **obj);

size_t SelvaObject_MemUsage(const void *value);

/**
 * Delete a key an its value from a SelvaObject.
 */
int SelvaObject_DelKeyStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);

/**
 * Delete a key an its value from a SelvaObject.
 */
int SelvaObject_DelKey(struct SelvaObject *obj, const struct selva_string *key_name);

/**
 * Check whether a key exists in a SelvaObject.
 */
int SelvaObject_ExistsStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);

/**
 * Check whether a key exists in a SelvaObject.
 */
int SelvaObject_Exists(struct SelvaObject *obj, const struct selva_string *key_name);

/**
 * Check if the top-level of the given key exists in obj.
 * The part after the first dot doesn't need to exist.
 */
int SelvaObject_ExistsTopLevel(struct SelvaObject *obj, const struct selva_string *key_name);

/**
 * @addtogroup selva_object_double
 * Double values.
 * @{
 */

int SelvaObject_GetDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double *out) __attribute__((access(write_only, 4)));
int SelvaObject_GetDouble(struct SelvaObject *obj, const struct selva_string *key_name, double *out) __attribute__((access(write_only, 3)));

/**
 * Set a double field value.
 * Can also handle array indexing.
 */
int SelvaObject_SetDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value);
int SelvaObject_SetDouble(struct SelvaObject *obj, const struct selva_string *key_name, double value);

/**
 * Set a double field to its default value if the field is unset.
 * Doesn't handle array indexing.
 */
int SelvaObject_SetDoubleDefaultStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value);
int SelvaObject_SetDoubleDefault(struct SelvaObject *obj, const struct selva_string *key_name, double value);

/**
 * Set a long long field to its default value if the field is unset.
 * Doesn't handle array indexing.
 */
int SelvaObject_UpdateDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value);
int SelvaObject_UpdateDouble(struct SelvaObject *obj, const struct selva_string *key_name, double value);

/**
 * Increment a double field value.
 * Can also handle array indexing.
 */
int SelvaObject_IncrementDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double default_value, double incr, double *new);
int SelvaObject_IncrementDouble(struct SelvaObject *obj, const struct selva_string *key_name, double default_value, double incr, double *new);

/**
 * @}
 */

/**
 * @addtogroup selva_object_long_long
 * Long long values.
 * @{
 */

int SelvaObject_GetLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long *out) __attribute__((access(write_only, 4)));
int SelvaObject_GetLongLong(struct SelvaObject *obj, const struct selva_string *key_name, long long *out) __attribute__((access(write_only, 3)));

/**
 * Set a long long field value.
 * Can also handle array indexing.
 */
int SelvaObject_SetLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value);
int SelvaObject_SetLongLong(struct SelvaObject *obj, const struct selva_string *key_name, long long value);

/**
 * Set a long long field to its default value if the field is unset.
 * Doesn't handle array indexing.
 */
int SelvaObject_SetLongLongDefaultStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value);
int SelvaObject_SetLongLongDefault(struct SelvaObject *obj, const struct selva_string *key_name, long long value);

/**
 * Update a long long field value.
 * Return SELVA_EEXIST if the current value equals value; Otherwise set value.
 */
int SelvaObject_UpdateLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value);
int SelvaObject_UpdateLongLong(struct SelvaObject *obj, const struct selva_string *key_name, long long value);

/**
 * Increment a long long field value.
 * Can also handle array indexing.
 */
int SelvaObject_IncrementLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long default_value, long long incr, long long *new);
int SelvaObject_IncrementLongLong(struct SelvaObject *obj, const struct selva_string *key_name, long long default_value, long long incr, long long *new);

/**
 * @}
 */

/**
 * @addtogroup selva_object_string
 * String values.
 * @{
 */

int SelvaObject_GetStringStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct selva_string **out) __attribute__((access(write_only, 4)));
int SelvaObject_GetString(struct SelvaObject *obj, const struct selva_string *key_name, struct selva_string **out) __attribute__((access(write_only, 3)));

/**
 * Set a string value.
 * Can also handle array indexing.
 * @param key_name is the name of the key ob obj. The argument is used only for lookup and does't need to be retained.
 * @param value is the value; the caller needs to make sure the string is retained.
 */
int SelvaObject_SetStringStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct selva_string *value);
int SelvaObject_SetString(struct SelvaObject *obj, const struct selva_string *key_name, struct selva_string *value);

/**
 * @}
 */

/**
 * @addtogroup selva_object_objects
 * Nested objects.
 * @{
 */

int SelvaObject_GetObjectStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct SelvaObject **out) __attribute__((access(write_only, 4)));
int SelvaObject_GetObject(struct SelvaObject *obj, const struct selva_string *key_name, struct SelvaObject **out) __attribute__((access(write_only, 3)));
int SelvaObject_SetObjectStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct SelvaObject *value);
int SelvaObject_SetObject(struct SelvaObject *obj, const struct selva_string *key_name, struct SelvaObject *in);

/**
 * @}
 */

/**
 * @addtogroup selva_object_hll
 * HyperLogLog fields.
 * @{
 */

/**
 * Add an element to a Hyperloglog field.
 * @returns 0 if the field was not modified; 1 if the field was modified; Otwherwise selva error;
 */
int SelvaObject_AddHllStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, const void *el, size_t el_size);
int SelvaObject_AddHll(struct SelvaObject *obj, const struct selva_string *key_name, const void *el, size_t el_size);

/**
 * @}
 */

/**
 * @addtogroup selva_object_sets
 * Sets.
 * @{
 */

int SelvaObject_AddDoubleSet(struct SelvaObject *obj, const struct selva_string *key_name, double value);
int SelvaObject_AddLongLongSet(struct SelvaObject *obj, const struct selva_string *key_name, long long value);
int SelvaObject_AddStringSet(struct SelvaObject *obj, const struct selva_string *key_name, struct selva_string *value);
int SelvaObject_RemDoubleSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value);
int SelvaObject_RemDoubleSet(struct SelvaObject *obj, const struct selva_string *key_name, double value);
int SelvaObject_RemLongLongSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value);
int SelvaObject_RemLongLongSet(struct SelvaObject *obj, const struct selva_string *key_name, long long value);
int SelvaObject_RemStringSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct selva_string *value);
int SelvaObject_RemStringSet(struct SelvaObject *obj, const struct selva_string *key_name, struct selva_string *value);
struct SelvaSet *SelvaObject_GetSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
struct SelvaSet *SelvaObject_GetSet(struct SelvaObject *obj, const struct selva_string *key_name);

/**
 * @}
 */

/**
 * @addtogroup selva_object_arrays
 * Arrays.
 * Functions specialized for handling arrays.
 * @{
 */

int SelvaObject_InsertArrayStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, void *p);
int SelvaObject_InsertArray(struct SelvaObject *obj, const struct selva_string *key_name, enum SelvaObjectType subtype, void *p);
int SelvaObject_AssignArrayIndexStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, ssize_t idx, void *p);
int SelvaObject_AssignArrayIndex(struct SelvaObject *obj, const struct selva_string *key_name, enum SelvaObjectType subtype, ssize_t idx, void *p);
int SelvaObject_InsertArrayIndexStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, ssize_t idx, void *p);
int SelvaObject_InsertArrayIndex(struct SelvaObject *obj, const struct selva_string *key_name, enum SelvaObjectType subtype, ssize_t idx, void *p);
int SelvaObject_RemoveArrayIndexStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, ssize_t idx);
int SelvaObject_RemoveArrayIndex(struct SelvaObject *obj, const struct selva_string *key_name, ssize_t idx);
int SelvaObject_GetArrayStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType *out_subtype, struct SVector **out_p) __attribute__((access(write_only, 5)));
int SelvaObject_GetArray(struct SelvaObject *obj, const struct selva_string *key_name, enum SelvaObjectType *out_subtype, struct SVector **out_p) __attribute__((access(write_only, 4)));
size_t SelvaObject_GetArrayLen(struct SelvaObject *obj, const struct selva_string *key_name);
size_t SelvaObject_GetArrayLenStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);

/**
 * @}
 */

/**
 * @addtogroup selva_object_pointer
 * Pointer values.
 * @{
 */

/**
 * Set a pointer value.
 * @param p is the pointer value and it must be non-NULL.
 * @param opts is an optional pointer to SELVA_OBJECT_POINTER ops that can define
 *             how to free the data pointed by the pointer or how to serialize it.
 */
int SelvaObject_SetPointerStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void *p, const struct SelvaObjectPointerOpts *opts) __attribute__((access(none, 4)));

/**
 * Set a pointer value.
 * @param p is the pointer value and it must be non-NULL.
 * @param opts is an optional pointer to SELVA_OBJECT_POINTER ops that can define
 *             how to free the data pointed by the pointer or how to serialize it.
 */
int SelvaObject_SetPointer(struct SelvaObject *obj, const struct selva_string *key_name, void *p, const struct SelvaObjectPointerOpts *opts) __attribute__((access(none, 3)));
int SelvaObject_GetPointerStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void **out_p) __attribute__((access(write_only, 4)));
int SelvaObject_GetPointer(struct SelvaObject *obj, const struct selva_string *key_name, void **out_p) __attribute__((access(write_only, 3)));

/**
 * Get partial match from key_name_str.
 * The length of the matching part is returned.
 * @returns Length of the matching key_name; Otherwise an error code is returned.
 */
int SelvaObject_GetPointerPartialMatchStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void **out_p) __attribute__((access(write_only, 4)));

/**
 * @}
 */

/**
 * @addtogroup selva_object_any
 * Get any functions.
 * @{
 */

int SelvaObject_GetAnyLangStr(struct SelvaObject *obj, struct selva_string *lang, const char *key_name_str, size_t key_name_len, struct SelvaObjectAny *res) __attribute__((access(write_only, 5)));
int SelvaObject_GetAnyLang(struct SelvaObject *obj, struct selva_string *lang, const struct selva_string *key_name, struct SelvaObjectAny *res) __attribute__((access(write_only, 4)));
int SelvaObject_GetAnyStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct SelvaObjectAny *res) __attribute__((access(write_only, 4)));
int SelvaObject_GetAny(struct SelvaObject *obj, const struct selva_string *key_name, struct SelvaObjectAny *res) __attribute__((access(write_only, 3)));

/**
 * @}
 */

/**
 * @addtogroup selva_object_type
 * Key type and length functions.
 * @{
 */

enum SelvaObjectType SelvaObject_GetTypeStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);
enum SelvaObjectType SelvaObject_GetType(struct SelvaObject *obj, const struct selva_string *key_name);

/**
 * Get the length of a SelvaObject or a key value.
 * Return value can be the number of elements or a byte size of the value,
 * depending on the exact type.
 */
ssize_t SelvaObject_LenStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len);

/**
 * Get the length of a SelvaObject or a key value.
 * Return value can be the number of elements or a byte size of the value,
 * depending on the exact type.
 */
ssize_t SelvaObject_Len(struct SelvaObject *obj, const struct selva_string *key_name);

/**
 * Get a string name of a SelvaObjectType.
 * @param type is the type.
 * @param len is an optional argument that will be set to the size of the string returned.
 */
const char *SelvaObject_Type2String(enum SelvaObjectType type, size_t *len);

/**
 * @}
 */

/**
 * @addtogroup selva_object_metadata
 * Key metadata.
 * @{
 */

int SelvaObject_GetUserMetaStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, SelvaObjectMeta_t *meta) __attribute__((access(write_only, 4)));
int SelvaObject_GetUserMeta(struct SelvaObject *obj, const struct selva_string *key_name, SelvaObjectMeta_t *meta) __attribute__((access(write_only, 3)));
int SelvaObject_SetUserMetaStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, SelvaObjectMeta_t meta, SelvaObjectMeta_t *old_meta) __attribute__((access(write_only, 5)));
int SelvaObject_SetUserMeta(struct SelvaObject *obj, const struct selva_string *key_name, SelvaObjectMeta_t meta, SelvaObjectMeta_t *old_meta) __attribute__((access(write_only, 4)));

/**
 * @}
 */

/**
 * @addtogroup selva_object_foreach
 * Foreach key iterators.
 * @{
 */

SelvaObject_Iterator *SelvaObject_ForeachBegin(struct SelvaObject *obj);
const char *SelvaObject_ForeachKey(const struct SelvaObject *obj, SelvaObject_Iterator **iterator);

union SelvaObjectArrayForeachValue {
    double d;
    long long ll;
    struct selva_string *s;
    struct SelvaObject *obj;
};

typedef int (*SelvaObjectArrayForeachCallback)(union SelvaObjectArrayForeachValue value, enum SelvaObjectType subtype, void *arg);

struct SelvaObjectArrayForeachCallback {
    SelvaObjectArrayForeachCallback cb;
    void * cb_arg;
};

union SelvaObjectSetForeachValue {
    struct selva_string *s;
    double d;
    long long ll;
    Selva_NodeId node_id;
};

typedef int (*SelvaObjectSetForeachCallback)(union SelvaObjectSetForeachValue value, enum SelvaSetType type, void *arg);

struct SelvaObjectSetForeachCallback {
    SelvaObjectSetForeachCallback cb;
    void *cb_arg;
};

/**
 * Foreach value of specified type in an object.
 * Visiting all subobjects can be achieved by using
 * SelvaObject_ForeachValueType() and recursing when a SELVA_OBJECT_OBJECT is
 * found.
 * @param name_out is a direct pointer to the name and it will be rendered invalid if the key is deleted.
 */
void *SelvaObject_ForeachValue(
        const struct SelvaObject *obj,
        SelvaObject_Iterator **iterator,
        const char **name_out,
        enum SelvaObjectType type);

/**
 * Foreach value in an object.
 * @param name_out is set to the name of the key found.
 * @param type_out is set to the type of the returned value.
 */
void *SelvaObject_ForeachValueType(
        const struct SelvaObject *obj,
        SelvaObject_Iterator **iterator,
        const char **name_out,
        enum SelvaObjectType *type_out);

/**
 * Foreach value in an array field of an object.
 */
int SelvaObject_ArrayForeach(
        struct SelvaObject *obj,
        const char *field_str,
        size_t field_len,
        const struct SelvaObjectArrayForeachCallback *cb);

/**
 * Foereach value in a set field of an object.
 */
int SelvaObject_SetForeach(
        struct SelvaObject *obj,
        const char *field_str,
        size_t field_len,
        const struct SelvaObjectSetForeachCallback *cb);

/**
 * @}
 */

/**
 * @addtogroup selva_object_selva_proto
 * Selva_proto reply functions.
 * @{
 */

/*
 * Send a SelvaObject as a selva_proto reply.
 * @param key_name_str can be NULL.
 */
int SelvaObject_ReplyWithObjectStr(
        struct selva_server_response_out *resp,
        struct selva_string *lang,
        struct SelvaObject *obj,
        const char *key_name_str,
        size_t key_name_len,
        enum SelvaObjectReplyFlags flags);

/*
 * Send a SelvaObject as a selva_proto reply.
 * @param key_name_str can be NULL.
 */
int SelvaObject_ReplyWithObject(
        struct selva_server_response_out *resp,
        struct selva_string *lang,
        struct SelvaObject *obj,
        const struct selva_string *key_name,
        enum SelvaObjectReplyFlags flags);

/**
 * Reply with a wildcard.
 * @param flags Accepts SELVA_OBJECT_REPLY_SPLICE_FLAG and other reply flags.
 */
int SelvaObject_ReplyWithWildcardStr(
        struct selva_server_response_out *resp,
        struct selva_string *lang,
        struct SelvaObject *obj,
        const char *okey_str,
        size_t okey_len,
        long *resp_count,
        int resp_path_start_idx,
        enum SelvaObjectReplyFlags flags);

/**
 * @}
 */

/**
 * @addtogroup selva_object_sdb
 * Object serialization.
 * @{
 */

/**
 * Load a SelvaObject.
 * @returns a SelvaObject.
 */
struct SelvaObject *SelvaObjectTypeLoad(struct selva_io *io, int encver, void *ptr_load_data);

struct SelvaObject *SelvaObjectTypeLoadTo(struct selva_io *io, int encver, struct SelvaObject *obj, void *ptr_load_data);

/**
 * Load a SelvaObject or NULL.
 * @returns NULL if the object length is zero.
 */
struct SelvaObject *SelvaObjectTypeLoad2(struct selva_io *io, int encver, void *ptr_load_data);

/**
 * Serialize a SelvaObject.
 * @param obj is a pointer to the SelvaObject to be serialized.
 * @param ptr_save_data is an optional pointer to additional data that can be used by a registered pointer type.
 */
void SelvaObjectTypeSave(struct selva_io *io, struct SelvaObject *obj, void *ptr_save_data);

/**
 * Serialize a SelvaObject or NULL.
 * @param obj is a pointer to the SelvaObject to be serialized. Can be NULL.
 * @param ptr_save_data is an optional pointer to additional data that can be used by a registered pointer type.
 */
void SelvaObjectTypeSave2(struct selva_io *io, struct SelvaObject *obj, void *ptr_save_data);

/**
 * @}
 */

#endif /* SELVA_OBJECT */
