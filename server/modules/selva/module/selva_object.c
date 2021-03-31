#include <assert.h>
#include <limits.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "cdefs.h"
#include "linker_set.h"
#include "typestr.h"
#include "cstrings.h"
#include "errors.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "svector.h"
#include "tree.h"

#define SELVA_OBJECT_ENCODING_VERSION   0
#define SELVA_OBJECT_KEY_MAX            USHRT_MAX
#define SELVA_OBJECT_SIZE_MAX           SIZE_MAX

#define SELVA_OBJECT_GETKEY_CREATE      0x1 /*!< Create the key and required nested objects. */
#define SELVA_OBJECT_GETKEY_DELETE      0x2 /*!< Delete the key found. */

RB_HEAD(SelvaObjectKeys, SelvaObjectKey);

struct SelvaObjectKey {
    enum SelvaObjectType type; /*!< Type of the value. */
    enum SelvaObjectType subtype; /*!< Subtype of the value. Arrays use this. */
    SelvaObjectMeta_t user_meta; /*!< User defined metadata. */
    unsigned short name_len;
    RB_ENTRY(SelvaObjectKey) _entry;
    union {
        struct {
            void *value; /*!< The rest of the types use this. */
            const struct SelvaObjectPointerOpts *ptr_opts; /*!< Options for a SELVA_OBJECT_POINTER */
        };
        double emb_double_value; /*!< SELVA_OBJECT_DOUBLE */
        long long emb_ll_value; /*!< SELVA_OBJECT_LONGLONG */
        struct SelvaSet selva_set; /*!< SELVA_OBJECT_SET */
        SVector array; /*!< SELVA_OBJECT_ARRAY */
    };
    char name[0]; /*!< Name of the key. */
};

struct SelvaObject {
    size_t obj_size;
    struct SelvaObjectKeys keys_head;
};

struct so_type_name {
    const char * const name;
    size_t len;
};

static RedisModuleType *ObjectType;
SET_DECLARE(selva_objpop, struct SelvaObjectPointerOpts);

/**
 * Defaults for SELVA_OBJECT_POINTER handling.
 * The default is: Do Nothing.
 */
static struct SelvaObjectPointerOpts default_ptr_opts = {
    .ptr_type_id = 0,
};
SELVA_OBJECT_POINTER_OPTS(default_ptr_opts);

static int get_key(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, unsigned flags, struct SelvaObjectKey **out);
static void replyWithKeyValue(RedisModuleCtx *ctx, RedisModuleString *lang, struct SelvaObjectKey *key);
static void replyWithObject(RedisModuleCtx *ctx, RedisModuleString *lang, struct SelvaObject *obj);
RB_PROTOTYPE_STATIC(SelvaObjectKeys, SelvaObjectKey, _entry, SelvaObject_Compare)

static int SelvaObject_Compare(const struct SelvaObjectKey *a, const struct SelvaObjectKey *b) {
    return strcmp(a->name, b->name);
}

RB_GENERATE_STATIC(SelvaObjectKeys, SelvaObjectKey, _entry, SelvaObject_Compare)

static const struct so_type_name type_names[] = {
    [SELVA_OBJECT_NULL] = { "null", 4 },
    [SELVA_OBJECT_DOUBLE] = { "double", 6 },
    [SELVA_OBJECT_LONGLONG] = { "long long", 9 },
    [SELVA_OBJECT_STRING] = { "string", 6 },
    [SELVA_OBJECT_OBJECT] = { "object", 6 },
    [SELVA_OBJECT_SET] = { "selva_set", 9 },
    [SELVA_OBJECT_ARRAY] = { "array", 7 },
};

struct SelvaObject *SelvaObject_New(void) {
    struct SelvaObject *obj;

    obj = RedisModule_Alloc(sizeof(*obj));
    obj->obj_size = 0;
    RB_INIT(&obj->keys_head);

    return obj;
}

static struct SelvaObjectPointerOpts *get_ptr_opts(unsigned ptr_type_id) {
    struct SelvaObjectPointerOpts **p;

    if (ptr_type_id >= SET_COUNT(selva_objpop)) {
        ptr_type_id = 0;
    }

    SET_FOREACH(p, selva_objpop) {
        struct SelvaObjectPointerOpts *opts = *p;

        if (opts->ptr_type_id == ptr_type_id) {
            return opts;
        }
    }

    __builtin_unreachable();
    return NULL; /* Never reached. */
}

static int clear_key_value(struct SelvaObjectKey *key) {
    switch (key->type) {
    case SELVA_OBJECT_NULL:
        /* NOP */
        break;
    case SELVA_OBJECT_DOUBLE:
        break;
    case SELVA_OBJECT_LONGLONG:
        break;
    case SELVA_OBJECT_STRING:
        if (key->value) {
            RedisModule_FreeString(NULL, key->value);
        }
        break;
    case SELVA_OBJECT_OBJECT:
        if (key->value) {
            struct SelvaObject *obj = (struct SelvaObject *)key->value;

            SelvaObject_Destroy(obj);
        }
        break;
    case SELVA_OBJECT_SET:
        SelvaSet_Destroy(&key->selva_set);
        break;
    case SELVA_OBJECT_ARRAY:
        /* TODO Clear array key */
        if (key->subtype == SELVA_OBJECT_STRING) {
            struct SVectorIterator it;
            RedisModuleString *str;

            SVector_ForeachBegin(&it, &key->array);
            while ((str = SVector_Foreach(&it))) {
                RedisModule_FreeString(NULL, str);
            }
        } else {
            fprintf(stderr, "%s: Key clear failed: Unsupported array type (%d)\n",
                    __FILE__, (int)key->subtype);
        }
        SVector_Destroy(&key->array);
        break;
    case SELVA_OBJECT_POINTER:
        if (key->ptr_opts && key->ptr_opts->ptr_free) {
            key->ptr_opts->ptr_free(key->value);
        }
        key->value = NULL; /* Not strictly necessary. */
        break;
    default:
        /*
         * In general default shouldn't be used because it may mask out missing
         * type handling but it's acceptable here.
         */
        fprintf(stderr, "%s: Unknown object value type (%d)\n", __FILE__, (int)key->type);
        return SELVA_EINTYPE;
    }

    key->type = SELVA_OBJECT_NULL;

    return 0;
}

void SelvaObject_Clear(struct SelvaObject *obj) {
    struct SelvaObjectKey *next;

    for (struct SelvaObjectKey *key = RB_MIN(SelvaObjectKeys, &obj->keys_head); key != NULL; key = next) {
        next = RB_NEXT(SelvaObjectKeys, &obj->keys_head, key);
        RB_REMOVE(SelvaObjectKeys, &obj->keys_head, key);
        obj->obj_size--;

        /* Clear and free the key. */
        (void)clear_key_value(key);
#if MEM_DEBUG
        memset(key, 0, sizeof(*key));
#endif
        RedisModule_Free(key);
    }
}

void SelvaObject_Destroy(struct SelvaObject *obj) {
    SelvaObject_Clear(obj);
#if MEM_DEBUG
    if (obj) {
        memset(obj, 0, sizeof(*obj));
    }
#endif
    RedisModule_Free(obj);
}

void _cleanup_SelvaObject_Destroy(struct SelvaObject **obj) {
    if (*obj) {
        SelvaObject_Destroy(*obj);
    }
}

size_t SelvaObject_MemUsage(const void *value) {
    struct SelvaObject *obj = (struct SelvaObject *)value;
    struct SelvaObjectKey *key;
    size_t size = sizeof(*obj);

    RB_FOREACH(key, SelvaObjectKeys, &obj->keys_head) {
        size += sizeof(*key) + key->name_len + 1;

        switch (key->type) {
        case SELVA_OBJECT_STRING:
            if (key->value) {
                size_t len;

                (void)RedisModule_StringPtrLen(key->value, &len);
                size += len + 1;
            }
            break;
        case SELVA_OBJECT_OBJECT:
            if (key->value) {
                size += SelvaObject_MemUsage(key->value);
            }
            break;
        default:
            /* 0 */
            break;
        }
    }

    return size;
}

static struct SelvaObject *SelvaObject_Open(RedisModuleCtx *ctx, RedisModuleString *key_name, int mode) {
    struct SelvaObject *obj = NULL;
    RedisModuleKey *key;
    int type;

    key = RedisModule_OpenKey(ctx, key_name, mode);
    type = RedisModule_KeyType(key);

    if (type != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != ObjectType) {
        RedisModule_CloseKey(key);
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);

        return NULL;
    }

    /* Create an empty value object if the key is currently empty. */
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        if ((mode & REDISMODULE_WRITE) == REDISMODULE_WRITE) {
            obj = SelvaObject_New();
            if (!obj) {
                replyWithSelvaError(ctx, SELVA_ENOMEM);
            }

            RedisModule_ModuleTypeSetValue(key, ObjectType, obj);
        } else {
            replyWithSelvaError(ctx, SELVA_ENOENT);
        }
    } else {
        obj = RedisModule_ModuleTypeGetValue(key);
        if (!obj) {
            replyWithSelvaError(ctx, SELVA_ENOENT);
        }
    }

    return obj;
}

int SelvaObject_Key2Obj(RedisModuleKey *key, struct SelvaObject **out) {
    struct SelvaObject *obj;

    if (!key) {
        return SELVA_ENOENT;
    }

    /* Create an empty value object if the key is currently empty. */
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        int err;

        obj = SelvaObject_New();
        if (!obj) {
            return SELVA_ENOMEM;
        }

        err = RedisModule_ModuleTypeSetValue(key, ObjectType, obj);
        if (err != REDISMODULE_OK) {
            SelvaObject_Destroy(obj);
            return SELVA_ENOENT;
        }
        /* TODO This check is really slow */
#if 0
    } else if (RedisModule_ModuleTypeGetType(key) == ObjectType) {
#endif
    } else {
        obj = RedisModule_ModuleTypeGetValue(key);
        if (!obj) {
            return SELVA_ENOENT;
        }
#if 0
    } else {
        return SELVA_EINVAL;
#endif
    }

    *out = obj;
    return 0;
}

static int get_key_obj(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, unsigned flags, struct SelvaObjectKey **out) {
    const char *sep = ".";
    const size_t nr_parts = substring_count(key_name_str, ".", key_name_len) + 1;
    char buf[key_name_len + 1]; /* We assume that the length has been sanity checked at this point. */
    char *s = buf;
    struct SelvaObjectKey *key = NULL;
    struct SelvaObject *cobj = obj; /* Containing object. */

    strncpy(s, key_name_str, key_name_len);
    s[key_name_len] = '\0';

    size_t nr_parts_found = 0;
    for (s = strtok(s, sep); s; s = strtok(NULL, sep)) {
        const size_t slen = strlen(s);
        int err;

        cobj = obj;
        key = NULL; /* This needs to be cleared on every iteration. */
        nr_parts_found++;
        err = get_key(obj, s, slen, 0, &key);
        if ((err == SELVA_ENOENT || (err == 0 && key->type != SELVA_OBJECT_OBJECT)) &&
            (flags & SELVA_OBJECT_GETKEY_CREATE)) {
            /*
             * Either the nested object doesn't exist yet or the nested key is not an object,
             * but we are allowed to create one here.
             */
            if (!key) {
                /*
                 * Only create the key if it didn't exist. Otherwise we can just
                 * reuse it.
                 */
                if (obj->obj_size == SELVA_OBJECT_SIZE_MAX) {
                    return SELVA_OBJECT_EOBIG;
                }

                const size_t key_size = sizeof(struct SelvaObjectKey) + slen + 1;
                key = RedisModule_Alloc(key_size);
                if (!key) {
                    return SELVA_ENOMEM;
                }

                memset(key, 0, key_size);
                strcpy(key->name, s); /* strok() is safe. */
                key->name_len = slen;
                obj->obj_size++;
                (void)RB_INSERT(SelvaObjectKeys, &obj->keys_head, key);
            } else {
                /*
                 * Clear the old value.
                 */
                clear_key_value(key);
            }
            key->type = SELVA_OBJECT_OBJECT;
            key->value = SelvaObject_New();

            if (!key->value) {
                /* A partial object might have been created! */
                key->type = SELVA_OBJECT_NULL;
                return SELVA_ENOMEM;
            }

            obj = key->value;
        } else if (err) {
            /*
             * Error, bail out.
             */
            return err;
        } else if (key->type == SELVA_OBJECT_OBJECT) {
            /*
             * Keep nesting or return an object if this was the last token.
             */
            obj = key->value;
        } else {
            /*
             * Found the final key.
             */
            break;
        }
    }

    /*
     * Check that we found what  we were really looking for. Consider the
     * following:
     * We have a key: a.b = "hello"
     * We do a lookup for "a.b.c" but end up to "a.b"
     * Without the following check we'd happily tell the user that the value of
     * "a.b.c" == "hello".
     */
    if (nr_parts_found != nr_parts || !key) {
        return SELVA_ENOENT;
    }

    if (flags & SELVA_OBJECT_GETKEY_DELETE) {
        RB_REMOVE(SelvaObjectKeys, &cobj->keys_head, key);
        cobj->obj_size--;
        (void)clear_key_value(key);
#if MEM_DEBUG
        memset(key, 0, sizeof(*key));
#endif
        RedisModule_Free(key);
        key = NULL;
    }

    *out = key;
    return 0;
}

static int get_key(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, unsigned flags, struct SelvaObjectKey **out) {
    struct SelvaObjectKey *filter;
    struct SelvaObjectKey *key;

    if (key_name_len + 1 > SELVA_OBJECT_KEY_MAX) {
        return SELVA_ENAMETOOLONG;
    }

    if (strnstr(key_name_str, ".", key_name_len)) {
        return get_key_obj(obj, key_name_str, key_name_len, flags, out);
    }

    const size_t key_size = sizeof(struct SelvaObjectKey) + key_name_len + 1;
    char buf[key_size] __attribute__((aligned(alignof(struct SelvaObjectKey)))); /* RFE This might be dumb */

    filter = (struct SelvaObjectKey *)buf;
    memset(filter, 0, key_size);
    memcpy(filter->name, key_name_str, key_name_len);
    filter->name_len = key_name_len;

    key = RB_FIND(SelvaObjectKeys, &obj->keys_head, filter);
    if (!key && (flags & SELVA_OBJECT_GETKEY_CREATE)) {
        if (obj->obj_size == SELVA_OBJECT_SIZE_MAX) {
            return SELVA_OBJECT_EOBIG;
        }

        key = RedisModule_Alloc(key_size);
        if (!key) {
            return SELVA_ENOMEM;
        }

        memcpy(key, filter, key_size);
        memset(&key->_entry, 0, sizeof(key->_entry)); /* RFE Might not be necessary. */
        obj->obj_size++;
        (void)RB_INSERT(SelvaObjectKeys, &obj->keys_head, key);
    } else if (!key) {
        return SELVA_ENOENT;
    }

    if (flags & SELVA_OBJECT_GETKEY_DELETE) {
        RB_REMOVE(SelvaObjectKeys, &obj->keys_head, key);
        obj->obj_size--;
        (void)clear_key_value(key);
#if MEM_DEBUG
        if (key) {
            memset(key, 0, sizeof(*key));
        }
#endif
        RedisModule_Free(key);
        key = NULL;
    }

    *out = key;
    return 0;
}

/**
 * Get key for modify without destroying the original value.
 */
static int get_key_modify(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct SelvaObjectKey **out) {
    struct SelvaObjectKey *key;
    int err;

    /*
     * Do get_key() first without create to avoid clearing the original value that we want to modify.
     * If we get a SELVA_ENOENT error we can safely create the key.
     */
    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err == SELVA_ENOENT) {
        err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    }
    if (err) {
        return err;
    }

    *out = key;
    return 0;
}

int SelvaObject_DelKeyStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    struct SelvaObjectKey *key;

    assert(obj);

    return get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_DELETE, &key);
}

int SelvaObject_DelKey(struct SelvaObject *obj, const RedisModuleString *key_name) {
    TO_STR(key_name);

    return SelvaObject_DelKeyStr(obj, key_name_str, key_name_len);
}

int SelvaObject_ExistsStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    struct SelvaObjectKey *key;

    assert(obj);

    return get_key(obj, key_name_str, key_name_len, 0, &key);
}

int SelvaObject_Exists(struct SelvaObject *obj, const RedisModuleString *key_name) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);

    assert(obj);

    return get_key(obj, key_name_str, key_name_len, 0, &key);
}

int SelvaObject_ExistsTopLevel(struct SelvaObject *obj, const RedisModuleString *key_name) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);

    assert(obj);

    return get_key(obj, key_name_str, strcspn(key_name_str, "."), 0, &key);
}

int SelvaObject_GetDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double *out) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    } else if (key->type != SELVA_OBJECT_DOUBLE) {
        return SELVA_EINTYPE;
    }

    *out = key->emb_double_value;

    return 0;
}

int SelvaObject_GetDouble(struct SelvaObject *obj, const RedisModuleString *key_name, double *out) {
    TO_STR(key_name);

    return SelvaObject_GetDoubleStr(obj, key_name_str, key_name_len, out);
}

int SelvaObject_GetLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long *out) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    } else if (key->type != SELVA_OBJECT_LONGLONG) {
        return SELVA_EINTYPE;
    }

    *out = key->emb_ll_value;

    return 0;
}

int SelvaObject_GetLongLong(struct SelvaObject *obj, const RedisModuleString *key_name, long long *out) {
    TO_STR(key_name);

    return SelvaObject_GetLongLongStr(obj, key_name_str, key_name_len, out);
}

int SelvaObject_GetStringStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, RedisModuleString **out) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    } else if (key->type != SELVA_OBJECT_STRING) {
        return SELVA_EINTYPE;
    }
    assert(key->value);

    *out = key->value;

    return 0;
}

int SelvaObject_GetString(struct SelvaObject *obj, const RedisModuleString *key_name, RedisModuleString **out) {
    TO_STR(key_name);

    return SelvaObject_GetStringStr(obj, key_name_str, key_name_len, out);
}

int SelvaObject_SetDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    err = clear_key_value(key);
    if (err) {
        return err;
    }

    key->type = SELVA_OBJECT_DOUBLE;
    key->emb_double_value = value;

    return 0;
}

int SelvaObject_SetDouble(struct SelvaObject *obj, const RedisModuleString *key_name, double value) {
    TO_STR(key_name);

    return SelvaObject_SetDoubleStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_SetLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    err = clear_key_value(key);
    if (err) {
        return err;
    }

    key->type = SELVA_OBJECT_LONGLONG;
    key->emb_ll_value = value;

    return 0;
}

int SelvaObject_SetLongLong(struct SelvaObject *obj, const RedisModuleString *key_name, long long value) {
    TO_STR(key_name);

    return SelvaObject_SetLongLongStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_SetStringStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, RedisModuleString *value) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    err = clear_key_value(key);
    if (err) {
        return err;
    }

    key->type = SELVA_OBJECT_STRING;
    key->value = value;

    return 0;
}

int SelvaObject_SetString(struct SelvaObject *obj, const RedisModuleString *key_name, RedisModuleString *value) {
    TO_STR(key_name);

    return SelvaObject_SetStringStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_GetObjectStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct SelvaObject **out) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    } else if (key->type != SELVA_OBJECT_OBJECT) {
        return SELVA_EINTYPE;
    }
    assert(key->value);

    *out = key->value;

    return 0;
}

int SelvaObject_GetObject(struct SelvaObject *obj, const RedisModuleString *key_name, struct SelvaObject **out) {
    TO_STR(key_name);

    return SelvaObject_GetObjectStr(obj, key_name_str, key_name_len, out);
}

static int get_selva_set_modify(struct SelvaObject *obj, const RedisModuleString *key_name, enum SelvaSetType type, struct SelvaSet **set_out) {
    TO_STR(key_name);
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key_modify(obj, key_name_str, key_name_len, &key);
    if (err) {
        return err;
    }

    if (key->type != SELVA_OBJECT_SET) {
        err = clear_key_value(key);
        if (err) {
            return err;
        }

        SelvaSet_Init(&key->selva_set, type);
        key->type = SELVA_OBJECT_SET;
    } else if (key->selva_set.type != type) {
        return SELVA_EINTYPE;
    }

    *set_out = &key->selva_set;
    return 0;
}

int SelvaObject_AddDoubleSet(struct SelvaObject *obj, const RedisModuleString *key_name, double value) {
    struct SelvaSet *selva_set;
    int err;

    err = get_selva_set_modify(obj, key_name, SELVA_SET_TYPE_DOUBLE, &selva_set);
    if (err) {
        return err;
    }

    return SelvaSet_Add(selva_set, value);
}

int SelvaObject_AddLongLongSet(struct SelvaObject *obj, const RedisModuleString *key_name, long long value) {
    struct SelvaSet *selva_set;
    int err;

    err = get_selva_set_modify(obj, key_name, SELVA_SET_TYPE_LONGLONG, &selva_set);
    if (err) {
        return err;
    }

    return SelvaSet_Add(selva_set, value);
}

int SelvaObject_AddStringSet(struct SelvaObject *obj, const RedisModuleString *key_name, RedisModuleString *value) {
    struct SelvaSet *selva_set;
    int err;

    err = get_selva_set_modify(obj, key_name, SELVA_SET_TYPE_RMSTRING, &selva_set);
    if (err) {
        return err;
    }

    return SelvaSet_Add(selva_set, value);
}

static int get_selva_set(struct SelvaObject *obj, const RedisModuleString *key_name, enum SelvaSetType type, struct SelvaSet **set_out) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    if (key->type != SELVA_OBJECT_SET) {
        return SELVA_EINVAL;
    }

    if (key->selva_set.type != type) {
        return SELVA_EINTYPE;
    }

    *set_out = &key->selva_set;
    return 0;
}

int SelvaObject_RemDoubleSet(struct SelvaObject *obj, const RedisModuleString *key_name, double value) {
    struct SelvaSet *selva_set;
    int err;

    err = get_selva_set(obj, key_name, SELVA_SET_TYPE_RMSTRING, &selva_set);
    if (err) {
        return err;
    }

    /* TODO Should we return SELVA_EINVAL if the element was not found? */
    SelvaSet_DestroyElement(SelvaSet_Remove(selva_set, value));

    return 0;
}

int SelvaObject_RemLongLongSet(struct SelvaObject *obj, const RedisModuleString *key_name, long long value) {
    struct SelvaSet *selva_set;
    int err;

    err = get_selva_set(obj, key_name, SELVA_SET_TYPE_LONGLONG, &selva_set);
    if (err) {
        return err;
    }

    /* TODO Should we return SELVA_EINVAL if the element was not found? */
    SelvaSet_DestroyElement(SelvaSet_Remove(selva_set, value));

    return 0;
}

int SelvaObject_RemStringSet(struct SelvaObject *obj, const RedisModuleString *key_name, RedisModuleString *value) {
    struct SelvaSet *selva_set;
    struct SelvaSetElement *el;
    int err;

    err = get_selva_set(obj, key_name, SELVA_SET_TYPE_RMSTRING, &selva_set);
    if (err) {
        return err;
    }

    el = SelvaSet_Remove(selva_set, value);
    if (!el) {
        return 0; /* TODO Should we return SELVA_EINVAL? */
    }
    RedisModule_FreeString(NULL, el->value_rms);
    SelvaSet_DestroyElement(el);

    return 0;
}

struct SelvaSet *SelvaObject_GetSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err || key->type != SELVA_OBJECT_SET) {
        return NULL;
    }

    return &key->selva_set;
}

struct SelvaSet *SelvaObject_GetSet(struct SelvaObject *obj, const RedisModuleString *key_name) {
    TO_STR(key_name);

    return SelvaObject_GetSetStr(obj, key_name_str, key_name_len);
}

int SelvaObject_AddArrayStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, void *p) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    if (subtype != SELVA_OBJECT_STRING) {
        return SELVA_EINTYPE;
    }

    err = get_key_modify(obj, key_name_str, key_name_len, &key);
    if (err) {
        return err;
    }

    /* TODO Should it fail if the subtype doesn't match? */
    if (key->type != SELVA_OBJECT_ARRAY || key->subtype != subtype) {
        err = clear_key_value(key);
        if (err) {
            return err;
        }

        /*
         * Type must be set before initializing the vector to avoid a situation
         * where we'd have a key with an unknown value type.
         */
        key->type = SELVA_OBJECT_ARRAY;
        key->subtype = subtype;

        if (!SVector_Init(&key->array, 1, NULL)) {
            return SELVA_ENOMEM;
        }
    }

    SVector_Insert(&key->array, p);

    return 0;
}

int SelvaObject_AddArray(struct SelvaObject *obj, const RedisModuleString *key_name, enum SelvaObjectType subtype, void *p) {
    TO_STR(key_name);

    return SelvaObject_AddArrayStr(obj, key_name_str, key_name_len, subtype, p);
}

int SelvaObject_GetArrayStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType *out_subtype, SVector **out_p) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    if (key->type != SELVA_OBJECT_ARRAY) {
        return SELVA_EINTYPE;
    }

    if (out_subtype) {
        *out_subtype = key->subtype;
    }
    if (out_p) {
        *out_p = &key->array;
    }

    return 0;
}

int SelvaObject_GetArray(struct SelvaObject *obj, const RedisModuleString *key_name, enum SelvaObjectType *out_subtype, SVector **out_p) {
    TO_STR(key_name);

    return SelvaObject_GetArrayStr(obj, key_name_str, key_name_len, out_subtype, out_p);
}

int SelvaObject_SetPointerStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void *p, struct SelvaObjectPointerOpts *opts) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    if (!p) {
        /* The value must be non-NULL. */
        return SELVA_EINVAL;
    }

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    err = clear_key_value(key);
    if (err) {
        return err;
    }

    key->type = SELVA_OBJECT_POINTER;
    key->value = p;
    key->ptr_opts = opts;

    return 0;
}

int SelvaObject_SetPointer(struct SelvaObject *obj, const struct RedisModuleString *key_name, void *p, struct SelvaObjectPointerOpts *opts) {
    TO_STR(key_name);

    return SelvaObject_SetPointerStr(obj, key_name_str, key_name_len, p, opts);
}

int SelvaObject_GetPointerStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void **out_p) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    if (key->type != SELVA_OBJECT_POINTER) {
        return SELVA_EINTYPE;
    }

    if (out_p) {
        *out_p = key->value;
    }

    return 0;
}

int SelvaObject_GetPointer(struct SelvaObject *obj, const struct RedisModuleString *key_name, void **out_p) {
    TO_STR(key_name);

    return SelvaObject_GetPointerStr(obj, key_name_str, key_name_len, out_p);
}

enum SelvaObjectType SelvaObject_GetTypeStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    struct SelvaObjectKey *key;
    enum SelvaObjectType type = SELVA_OBJECT_NULL;
    int err;

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (!err) {
        type = key->type;
    }

    return type;
}

enum SelvaObjectType SelvaObject_GetType(struct SelvaObject *obj, const RedisModuleString *key_name) {
    TO_STR(key_name);

    if (unlikely(!key_name_str)) {
        return SELVA_OBJECT_NULL;
    }

    return SelvaObject_GetTypeStr(obj, key_name_str, key_name_len);
}

ssize_t SelvaObject_LenStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    struct SelvaObjectKey *key;
    int err;

    if (!key_name_str) {
        return obj->obj_size;
    }

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    switch (key->type) {
    case SELVA_OBJECT_NULL:
        return 0;
    case SELVA_OBJECT_DOUBLE:
    case SELVA_OBJECT_LONGLONG:
        return 1;
    case SELVA_OBJECT_STRING:
        if (key->value) {
            size_t len;

            (void)RedisModule_StringPtrLen(key->value, &len);
            return len;
        } else {
            return 0;
        }
    case SELVA_OBJECT_OBJECT:
        if (key->value) {
            struct SelvaObject *obj2 = (struct SelvaObject *)key->value;

            return obj2->obj_size;
        } else {
            return 0;
        }
    case SELVA_OBJECT_SET:
        return key->selva_set.size;
    case SELVA_OBJECT_ARRAY:
        return SVector_Size(&key->array);
    case SELVA_OBJECT_POINTER:
        return (key->ptr_opts && key->ptr_opts->ptr_len) ? key->ptr_opts->ptr_len(key->value) : 1;
    }

    return SELVA_EINTYPE;
}

ssize_t SelvaObject_Len(struct SelvaObject *obj, const RedisModuleString *key_name) {
    if (!key_name) {
        return obj->obj_size;
    }

    TO_STR(key_name);

    return SelvaObject_LenStr(obj, key_name_str, key_name_len);
}

int SelvaObjet_GetUserMetaStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, SelvaObjectMeta_t *meta) {
    int err;
    struct SelvaObjectKey *key;

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    *meta = key->user_meta;
    return 0;
}

int SelvaObjet_GetUserMeta(struct SelvaObject *obj, const RedisModuleString *key_name, SelvaObjectMeta_t *meta) {
    TO_STR(key_name);

    return SelvaObjet_GetUserMetaStr(obj, key_name_str, key_name_len, meta);
}

int SelvaObject_SetUserMetaStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, SelvaObjectMeta_t meta) {
    int err;
    struct SelvaObjectKey *key;

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    key->user_meta = meta;
    return 0;
}

int SelvaObject_SetUserMeta(struct SelvaObject *obj, const RedisModuleString *key_name, SelvaObjectMeta_t meta) {
    TO_STR(key_name);

    return SelvaObject_SetUserMetaStr(obj, key_name_str, key_name_len, meta);
}

/* TODO Support nested objects */
void *SelvaObject_ForeachBegin(struct SelvaObject *obj) {
    return RB_MIN(SelvaObjectKeys, &obj->keys_head);
}

const char *SelvaObject_ForeachKey(struct SelvaObject *obj, void **iterator) {
    struct SelvaObjectKey *key = *iterator;
    (void)obj; /* This makes the compiler think we are actually using obj. */

    if (!key) {
        return NULL;
    }

    *iterator = RB_NEXT(SelvaObjectKeys, &obj->keys_head, key);

    return key->name;
}

const void *SelvaObject_ForeachValue(struct SelvaObject *obj, void **iterator, const char **name_out, enum SelvaObjectType type) {
    struct SelvaObjectKey *key;
    (void)obj; /* This makes the compiler think we are actually using obj. */

    do {
        key = *iterator;
        if (!key) {
            return NULL;
        }

        *iterator = RB_NEXT(SelvaObjectKeys, &obj->keys_head, key);
    } while (key->type != type);

    if (name_out) {
        *name_out = key->name;
    }

    switch (key->type) {
    case SELVA_OBJECT_NULL:
        return NULL;
    case SELVA_OBJECT_DOUBLE:
        return &key->emb_double_value;
    case SELVA_OBJECT_LONGLONG:
        return &key->emb_ll_value;
    case SELVA_OBJECT_STRING:
    case SELVA_OBJECT_OBJECT:
    case SELVA_OBJECT_POINTER:
        return key->value;
    case SELVA_OBJECT_SET:
        return &key->selva_set;
    case SELVA_OBJECT_ARRAY:
        return &key->array;
    }

    return NULL;
}

const char *SelvaObject_Type2String(enum SelvaObjectType type, size_t *len) {
    if (type >= 0 && type < num_elem(type_names)) {
        const struct so_type_name *tn = &type_names[type];

        if (len) {
            *len = tn->len;
        }
        return tn->name;
    }

    return NULL;
}

int SelvaObject_DelCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    int err;

    const int ARGV_KEY = 1;
    const int ARGV_OKEY = 2;

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return REDISMODULE_OK;
    }

    err = SelvaObject_DelKey(obj, argv[ARGV_OKEY]);
    if (err == SELVA_ENOENT) {
        RedisModule_ReplyWithLongLong(ctx, 0);
    } else if (err) {
        return replyWithSelvaError(ctx, err);
    } else {
        RedisModule_ReplyWithLongLong(ctx, 1);
    }

    return RedisModule_ReplicateVerbatim(ctx);
}

int SelvaObject_ExistsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    int err;

    const int ARGV_KEY = 1;
    const int ARGV_OKEY = 2;

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return REDISMODULE_OK;
    }

    err = SelvaObject_Exists(obj, argv[ARGV_OKEY]);
    if (err == SELVA_ENOENT) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    } else if (err) {
        return replyWithSelvaError(ctx, err);
    }
    return RedisModule_ReplyWithLongLong(ctx, 1);
}

static void replyWithSelvaSet(RedisModuleCtx *ctx, struct SelvaSet *set) {
    struct SelvaSetElement *el;
    size_t n = 0;

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    if (set->type == SELVA_SET_TYPE_RMSTRING) {
        SELVA_SET_RMS_FOREACH(el, set) {
            RedisModule_ReplyWithString(ctx, el->value_rms);
            n++;
        }
    } else if (set->type == SELVA_SET_TYPE_DOUBLE) {
        SELVA_SET_DOUBLE_FOREACH(el, set) {
            RedisModule_ReplyWithDouble(ctx, el->value_d);
            n++;
        }
    } else if (set->type == SELVA_SET_TYPE_LONGLONG) {
        SELVA_SET_LONGLONG_FOREACH(el, set) {
            RedisModule_ReplyWithLongLong(ctx, el->value_ll);
            n++;
        }
    }

    RedisModule_ReplySetArrayLength(ctx, n);
}

static void replyWithArray(RedisModuleCtx *ctx, enum SelvaObjectType subtype, SVector *array) {
    /* TODO add selva_object_array reply support */
    (void)replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "Array type not supported");
}

static void replyWithKeyValue(RedisModuleCtx *ctx, RedisModuleString *lang, struct SelvaObjectKey *key) {
    switch (key->type) {
    case SELVA_OBJECT_NULL:
        RedisModule_ReplyWithNull(ctx);
        break;
    case SELVA_OBJECT_DOUBLE:
        RedisModule_ReplyWithDouble(ctx, key->emb_double_value);
        break;
    case SELVA_OBJECT_LONGLONG:
        RedisModule_ReplyWithLongLong(ctx, key->emb_ll_value);
        break;
    case SELVA_OBJECT_STRING:
        if (key->value) {
            RedisModule_ReplyWithString(ctx, key->value);
        } else {
            RedisModule_ReplyWithNull(ctx);
        }
        break;
    case SELVA_OBJECT_OBJECT:
        if (key->value) {
            if (key->user_meta == 2) {
                TO_STR(lang);
                if (lang_len > 0) {
                    struct SelvaObjectKey *key;
                    int err = get_key(key->value, lang_str, lang_len, 0, &key);
                    // ignore errors on purpose
                    if (!err) {
                        RedisModule_ReplyWithString(ctx, key->value);
                    }
                    break;
                }
            }

            replyWithObject(ctx, lang, key->value);
        } else {
            RedisModule_ReplyWithNull(ctx);
        }
        break;
    case SELVA_OBJECT_SET:
        replyWithSelvaSet(ctx, &key->selva_set);
        break;
    case SELVA_OBJECT_ARRAY:
        replyWithArray(ctx, key->subtype, &key->array);
        break;
    case SELVA_OBJECT_POINTER:
        RedisModule_ReplyWithStringBuffer(ctx, "(pointer)", 9);
        break;
    default:
        (void)replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "Type not supported %d", (int)key->type);
    }
}

static void replyWithObject(RedisModuleCtx *ctx, RedisModuleString *lang, struct SelvaObject *obj) {
    struct SelvaObjectKey *key;
    size_t n = 0;

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    RB_FOREACH(key, SelvaObjectKeys, &obj->keys_head) {
        RedisModule_ReplyWithStringBuffer(ctx, key->name, key->name_len);
        replyWithKeyValue(ctx, lang, key);

        n += 2;
    }

    RedisModule_ReplySetArrayLength(ctx, n);
}

int SelvaObject_ReplyWithObject(RedisModuleCtx *ctx, RedisModuleString *lang, struct SelvaObject *obj, const RedisModuleString *key_name) {
    struct SelvaObjectKey *key;
    int err;

    if (!key_name) {
        replyWithObject(ctx, lang, obj);
        return 0;
    }

    TO_STR(key_name);
    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    replyWithKeyValue(ctx, lang, key);

    return 0;
}

int SelvaObject_GetWithWildcardStr(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        struct SelvaObject *obj,
        const char *okey_str,
        size_t okey_len,
        long *resp_count,
        int resp_path_start_idx,
        unsigned int flags) {
    const size_t idx = strnstr(okey_str, ".*.", okey_len) - okey_str + 1; // .*. => *.

    /* path before the wildcard character */
    const size_t before_len = idx - 1;
    const char *before = okey_str;

    /* path after the wildcard character */
    const size_t after_len = okey_len - idx - 2;
    const char *after = &okey_str[idx + 2];

    struct SelvaObjectKey *key;
    int err;

    err = get_key(obj, before, before_len, 0, &key);
    if (!err && key->type == SELVA_OBJECT_OBJECT && key->user_meta == 1) {
        void *it = SelvaObject_ForeachBegin(key->value);

        const char *obj_key_name_str;
        while ((obj_key_name_str = SelvaObject_ForeachKey(key->value, &it))) {
            /*
                construct a new field path with the resolved path with the following:
                -> path before the wildcard
                -> the current object key being iterated
                -> path after the wildcard
            */
            const size_t obj_key_len = strlen(obj_key_name_str);
            const size_t new_field_len = before_len + 1 + obj_key_len + 1 + after_len;
            char new_field[new_field_len];
            sprintf(new_field, "%.*s.%.*s.%.*s",
                    (int)before_len, before,
                    (int)obj_key_len, obj_key_name_str,
                    (int)after_len, after);

            if (strnstr(new_field, ".*.", new_field_len)) {
                /* recurse for nested wildcards while keeping the resolved path */
                SelvaObject_GetWithWildcardStr(ctx, lang, obj, new_field, new_field_len, resp_count, resp_path_start_idx == -1 ? idx : resp_path_start_idx, flags);
                continue;
            }

            struct SelvaObjectKey *key;
            err = get_key(obj, new_field, new_field_len, 0, &key);

            if (flags == 1) {
                /* if the path should be spliced to start from the first wildcard (as expected by selva.object.get */
                const size_t reply_path_len = resp_path_start_idx == -1 ? obj_key_len + 1 + key->name_len : (before_len - resp_path_start_idx) + 1 + obj_key_len + 1 + key->name_len;
                char reply_path[reply_path_len];
                if (resp_path_start_idx == -1) {
                    sprintf(
                        reply_path, "%.*s.%.*s",
                        (int)obj_key_len, obj_key_name_str,
                        (int)key->name_len, key->name);
                } else {
                    sprintf(reply_path, "%.*s.%.*s.%.*s",
                        (int)before_len - resp_path_start_idx, before + resp_path_start_idx,
                        (int)obj_key_len, obj_key_name_str,
                        (int)key->name_len, key->name);
                }
                RedisModule_ReplyWithStringBuffer(ctx, reply_path, reply_path_len);
            } else {
                /* if the whole resolved path should be returned */
                const size_t reply_path_len = before_len + 1 + obj_key_len + 1 + key->name_len;
                char reply_path[reply_path_len];
                sprintf(reply_path, "%.*s.%.*s.%.*s",
                        (int)before_len, before,
                        (int)obj_key_len, obj_key_name_str,
                        (int)key->name_len, key->name);
                RedisModule_ReplyWithStringBuffer(ctx, reply_path, reply_path_len);
            }

            replyWithKeyValue(ctx, lang, key);
            *resp_count += 2;
        }
    } else {
        err = SELVA_ENOENT;
    }

    /* ignore errors unless nothing was returned and an error occurred */
    if (err && *resp_count == 0) {
        return err;
    }

    return 0;
}

int SelvaObject_GetCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    struct SelvaObjectKey *key;

    const int ARGV_LANG = 1;
    const int ARGV_KEY = 2;
    const int ARGV_OKEY = 3;

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *lang = argv[ARGV_LANG];
    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return REDISMODULE_OK;
    }

    if (argc == 3) {
        replyWithObject(ctx, lang, obj);
        return REDISMODULE_OK;
    }

    for (int i = ARGV_OKEY; i < argc; i++) {
        int is_wildcard = 0;
        const RedisModuleString *okey = argv[i];
        TO_STR(okey);

        int err = 0;

        if (strstr(okey_str, ".*.")) {
            is_wildcard = 1;

            long resp_count = 0;
            RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
            err = SelvaObject_GetWithWildcardStr(ctx, lang, obj, okey_str, okey_len, &resp_count, -1, 1);
            RedisModule_ReplySetArrayLength(ctx, resp_count);
        } else {
            err = get_key(obj, okey_str, okey_len, 0, &key);
        }

        if (err == SELVA_ENOENT) {
            /* Keep looking. */
            continue;
        } else if (err) {
            return replyWithSelvaErrorf(ctx, err, "get_key");
        }

        if (!is_wildcard) {
            replyWithKeyValue(ctx, lang, key);
        }
        return REDISMODULE_OK;
    }

    return RedisModule_ReplyWithNull(ctx);
}

int SelvaObject_SetCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    size_t values_set = 0;
    int err;

    const int ARGV_KEY = 1;
    const int ARGV_OKEY = 2;
    const int ARGV_TYPE = 3;
    const int ARGV_OVAL = 4;

    if (argc <= ARGV_TYPE) {
        return RedisModule_WrongArity(ctx);
    }

    size_t type_len;
    const char type = RedisModule_StringPtrLen(argv[ARGV_TYPE], &type_len)[0];

    if (type_len != 1) {
        return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "Invalid or missing type argument");
    }

    if (!(argc == 5 || (type == 'S' && argc >= 5))) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!obj) {
        return REDISMODULE_OK;
    }

    switch (type) {
    case 'f': /* SELVA_OBJECT_DOUBLE */
        err = SelvaObject_SetDouble(
            obj,
            argv[ARGV_OKEY],
            strtod(RedisModule_StringPtrLen(argv[ARGV_OVAL], NULL), NULL));
        values_set++;
        break;
    case 'i': /* SELVA_OBJECT_LONGLONG */
        err = SelvaObject_SetLongLong(
            obj,
            argv[ARGV_OKEY],
            strtoll(RedisModule_StringPtrLen(argv[ARGV_OVAL], NULL), NULL, 10));
        values_set++;
        break;
    case 's': /* SELVA_OBJECT_STRING */
        err = SelvaObject_SetString(obj, argv[ARGV_OKEY], argv[ARGV_OVAL]);
        if (err == 0) {
            RedisModule_RetainString(ctx, argv[ARGV_OVAL]);
        }
        values_set++;
        break;
    case 'S': /* SELVA_OBJECT_SET */
        for (int i = ARGV_OVAL; i < argc; i++) {
            if (SelvaObject_AddStringSet(obj, argv[ARGV_OKEY], argv[i]) == 0) {
                RedisModule_RetainString(ctx, argv[i]);
                values_set++;
            }
        }
        err = 0;
        break;
    default:
        err = SELVA_EINTYPE;
    }
    if (err) {
        return replyWithSelvaError(ctx, err);
    }
    RedisModule_ReplyWithLongLong(ctx, values_set);

    return RedisModule_ReplicateVerbatim(ctx);
}

int SelvaObject_TypeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    struct SelvaObjectKey *key;
    int err;

    const int ARGV_KEY = 1;
    const int ARGV_OKEY = 2;

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return REDISMODULE_OK;
    }

    enum SelvaObjectType type = SELVA_OBJECT_NULL;
    const RedisModuleString *okey = argv[ARGV_OKEY];
    TO_STR(okey);

    err = get_key(obj, okey_str, okey_len, 0, &key);
    if (!err) {
        type = key->type;
    } else if (err != SELVA_ENOENT) {
        return replyWithSelvaErrorf(ctx, err, "get_key");
    }

    if (type >= 0 && type < num_elem(type_names)) {
        const struct so_type_name *tn = &type_names[type];

        RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
        RedisModule_ReplyWithStringBuffer(ctx, tn->name, tn->len);

        if (type == SELVA_OBJECT_ARRAY) {
            enum SelvaObjectType subtype = key->subtype;

            if (subtype >= 0 && subtype < num_elem(type_names)) {
                const struct so_type_name *sub_tn = &type_names[subtype];

                RedisModule_ReplyWithStringBuffer(ctx, sub_tn->name, sub_tn->len);
            } else {
                replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "invalid key subtype %d", (int)subtype);
            }

            RedisModule_ReplySetArrayLength(ctx, 2);
        } else if (type == SELVA_OBJECT_SET) {
            RedisModule_ReplyWithLongLong(ctx, key->selva_set.type); /* TODO Type as a string */
            RedisModule_ReplySetArrayLength(ctx, 2);
        } else {
            RedisModule_ReplySetArrayLength(ctx, 1);
        }
    } else {
        return replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "invalid key type %d", (int)type);
    }

    return REDISMODULE_OK;
}

int SelvaObject_LenCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;

    const int ARGV_KEY = 1;
    const int ARGV_OKEY = 2;

    if (argc != 2 && argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return REDISMODULE_OK;
    }

    ssize_t len;
    if (argc == 2) {
        len = obj->obj_size;
    } else {
        len = SelvaObject_Len(obj, argv[ARGV_OKEY]);
        if (len < 0) {
            int err = (int)len;

            if (err == SELVA_EINTYPE) {
                return replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "key type not supported");
            } else {
                return replyWithSelvaError(ctx, err);
            }
        }
    }

    return RedisModule_ReplyWithLongLong(ctx, len);
}

int SelvaObject_GetMetaCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    int err;

    const int ARGV_KEY = 1;
    const int ARGV_OKEY = 2;

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return REDISMODULE_OK;
    }

    SelvaObjectMeta_t user_meta;
    err = SelvaObjet_GetUserMeta(obj, argv[ARGV_OKEY], &user_meta);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Failed to get key metadata");
    }

    return RedisModule_ReplyWithLongLong(ctx, user_meta);
}

int SelvaObject_SetMetaCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    SelvaObjectMeta_t user_meta;
    int err;

    const int ARGV_KEY = 1;
    const int ARGV_OKEY = 2;
    const int ARGV_MVAL = 3;

    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *mval = argv[ARGV_MVAL];
    TO_STR(mval);

    if (mval_len < sizeof(SelvaObjectMeta_t)) {
        return replyWithSelvaErrorf(ctx, SELVA_EINTYPE,"Expected: %s", typeof_str(user_meta));
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return REDISMODULE_OK;
    }

    memcpy(&user_meta, mval_str, sizeof(SelvaObjectMeta_t));
    err = SelvaObject_SetUserMeta(obj, argv[ARGV_OKEY], user_meta);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Failed to set key metadata");
    }

    return RedisModule_ReplyWithLongLong(ctx, 1);
}

static int rdb_load_object_double(RedisModuleIO *io, struct SelvaObject *obj, const RedisModuleString *name) {
    double value;
    int err;

    value = RedisModule_LoadDouble(io);
    err = SelvaObject_SetDouble(obj, name, value);
    if (err) {
        RedisModule_LogIOError(io, "warning", "Error while loading a double");
        return SELVA_EINVAL;
    }

    return 0;
}

static int rdb_load_object_long_long(RedisModuleIO *io, struct SelvaObject *obj, const RedisModuleString *name) {
    long long value;
    int err;

    value = RedisModule_LoadSigned(io);
    err = SelvaObject_SetLongLong(obj, name, value);
    if (err) {
        RedisModule_LogIOError(io, "warning", "Error while loading a long long");
        return SELVA_EINVAL;
    }

    return 0;
}

static int rdb_load_object_string(RedisModuleIO *io, struct SelvaObject *obj, const RedisModuleString *name) {
    RedisModuleString *value = RedisModule_LoadString(io);
    int err;

    err = SelvaObject_SetString(obj, name, value);
    if (err) {
        RedisModule_LogIOError(io, "warning", "Error while loading a string");
        return SELVA_EINVAL;
    }

    return 0;
}

static int rdb_load_object_set(RedisModuleIO *io, struct SelvaObject *obj, const RedisModuleString *name) {
    enum SelvaSetType setType = RedisModule_LoadUnsigned(io);
    const size_t n = RedisModule_LoadUnsigned(io);

    if (setType == SELVA_SET_TYPE_RMSTRING) {
        for (size_t j = 0; j < n; j++) {
            RedisModuleString *value = RedisModule_LoadString(io);

            SelvaObject_AddStringSet(obj, name, value);
        }
    } else if (setType == SELVA_SET_TYPE_DOUBLE) {
        for (size_t j = 0; j < n; j++) {
            double value = RedisModule_LoadDouble(io);

            SelvaObject_AddDoubleSet(obj, name, value);
        }
    } else if (setType == SELVA_SET_TYPE_LONGLONG) {
        for (size_t j = 0; j < n; j++) {
            long long value = RedisModule_LoadSigned(io);

            SelvaObject_AddLongLongSet(obj, name, value);
        }
    } else {
        RedisModule_LogIOError(io, "warning", "Unknown set type");
        return SELVA_EINTYPE;
    }

    return 0;
}

static int rdb_load_pointer(RedisModuleIO *io, int encver, struct SelvaObject *obj, const RedisModuleString *name, void *ptr_load_data) {
    unsigned ptr_type_id;

    ptr_type_id = RedisModule_LoadUnsigned(io);
    if (ptr_type_id > 0) {
        struct SelvaObjectPointerOpts *opts;
        int err;
        void *p;

        opts = get_ptr_opts(ptr_type_id);
        if (!(opts && opts->ptr_load)) {
            return SELVA_EINVAL; /* Presumably a serialized pointer should have a loader fn. */
        }

        p = opts->ptr_load(io, encver, ptr_load_data);
        if (!p) {
            RedisModule_LogIOError(io, "warning", "Failed to load a SELVA_OBJECT_POINTER");
            return SELVA_EGENERAL;
        }

        err = SelvaObject_SetPointer(obj, name, p, opts);
        if (err) {
            RedisModule_LogIOError(io, "warning", "Failed to load a SELVA_OBJECT_POINTER");
            return SELVA_EGENERAL;
        }
    } else {
        RedisModule_LogIOError(io, "warning", "No ptr_load given");
    }

    return 0;
}

static void *rdb_load_object(RedisModuleIO *io, int encver, void *ptr_load_data) {
    struct SelvaObject *obj;

    obj = SelvaObject_New();
    if (!obj) {
        RedisModule_LogIOError(io, "warning", "Failed to create a new SelvaObject");
        return NULL;
    }

    const size_t obj_size = RedisModule_LoadUnsigned(io);
    for (size_t i = 0; i < obj_size; i++) {
        RedisModuleString *name = RedisModule_LoadString(io);
        const enum SelvaObjectType type = RedisModule_LoadUnsigned(io);
        const SelvaObjectMeta_t user_meta = RedisModule_LoadUnsigned(io);

        if (unlikely(!name)) {
            RedisModule_LogIOError(io, "warning", "SelvaObject key name cannot be NULL");
            break;
        }

        switch (type) {
        case SELVA_OBJECT_NULL:
            /* NOP - There is generally no reason to recreate NULLs */
            break;
        case SELVA_OBJECT_DOUBLE:
            if(rdb_load_object_double(io, obj, name)) {
                return NULL;
            }
            break;
        case SELVA_OBJECT_LONGLONG:
            if (rdb_load_object_long_long(io, obj, name)) {
                return NULL;
            }
            break;
        case SELVA_OBJECT_STRING:
            if (rdb_load_object_string(io, obj, name)) {
                return NULL;
            }
            break;
        case SELVA_OBJECT_OBJECT:
            {
                struct SelvaObjectKey *key;
                TO_STR(name);
                int err;

                err = get_key(obj, name_str, name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
                if (err) {
                    RedisModule_LogIOError(io, "warning", "Error while creating an object key");
                    return NULL;
                }

                key->value = rdb_load_object(io, encver, ptr_load_data);
                if (!key->value) {
                    RedisModule_LogIOError(io, "warning", "Error while loading an object");
                    return NULL;
                }
                key->type = SELVA_OBJECT_OBJECT;
            }
            break;
        case SELVA_OBJECT_SET:
            if (rdb_load_object_set(io, obj, name)) {
                return NULL;
            }
            break;
        case SELVA_OBJECT_ARRAY:
            /* TODO Support arrays */
            RedisModule_LogIOError(io, "warning", "Array not supported in RDB");
            break;
        case SELVA_OBJECT_POINTER:
            if (rdb_load_pointer(io, encver, obj, name, ptr_load_data)) {
                return NULL;
            }
            break;
        default:
            RedisModule_LogIOError(io, "warning", "Unknown type");
        }

        /*
         * Not the most efficient way to do this as we may need to look
         * multiple lookups.
         */
        if (SelvaObject_SetUserMeta(obj, name, user_meta)) {
            RedisModule_LogIOError(io, "warning", "Failed to set user meta");
        }

        RedisModule_FreeString(NULL, name);
    }

    return obj;
}

struct SelvaObject *SelvaObjectTypeRDBLoad(RedisModuleIO *io, int encver, void *ptr_load_data) {
    struct SelvaObject *obj;

    if (encver != SELVA_OBJECT_ENCODING_VERSION) {
        /*
         * RFE
         * We should actually log an error here, or try to implement
         * the ability to load older versions of our data structure.
         */
        return NULL;
    }

    obj = rdb_load_object(io, encver, ptr_load_data);

    return obj;
}

void *_SelvaObjectTypeRDBLoad(RedisModuleIO *io, int encver) {
    return SelvaObjectTypeRDBLoad(io, encver, NULL);
}

static void rdb_save_object_string(RedisModuleIO *io, struct SelvaObjectKey *key) {
    if (!key->value) {
        RedisModule_LogIOError(io, "warning", "STRING value missing");
        return;
    }
    RedisModule_SaveString(io, key->value);
}

static void rdb_save_object_set(RedisModuleIO *io, struct SelvaObjectKey *key) {
    struct SelvaSet *selva_set = &key->selva_set;

    RedisModule_SaveUnsigned(io, selva_set->type);
    RedisModule_SaveUnsigned(io, selva_set->size);

    if (selva_set->type == SELVA_SET_TYPE_RMSTRING) {
        struct SelvaSetElement *el;

        SELVA_SET_RMS_FOREACH(el, &key->selva_set) {
            RedisModule_SaveString(io, el->value_rms);
        }
    } else if (selva_set->type == SELVA_SET_TYPE_DOUBLE) {
        struct SelvaSetElement *el;

        SELVA_SET_DOUBLE_FOREACH(el, &key->selva_set) {
            RedisModule_SaveDouble(io, el->value_d);
        }
    } else if (selva_set->type == SELVA_SET_TYPE_LONGLONG) {
        struct SelvaSetElement *el;

        SELVA_SET_LONGLONG_FOREACH(el, &key->selva_set) {
            RedisModule_SaveSigned(io, el->value_ll);
        }
    } else {
        RedisModule_LogIOError(io, "warning", "Unknown set type");
    }
}

void SelvaObjectTypeRDBSave(RedisModuleIO *io, struct SelvaObject *obj, void *ptr_save_data) {
    struct SelvaObjectKey *key;

    if (unlikely(!obj)) {
        RedisModule_LogIOError(io, "warning", "value can't be NULL");
        return;
    }

    RedisModule_SaveUnsigned(io, obj->obj_size);
    RB_FOREACH(key, SelvaObjectKeys, &obj->keys_head) {
        RedisModule_SaveStringBuffer(io, key->name, key->name_len);
        RedisModule_SaveUnsigned(io, key->type);
        RedisModule_SaveUnsigned(io, key->user_meta);

        switch (key->type) {
        case SELVA_OBJECT_NULL:
            /* null is implicit value and doesn't need to be persisted. */
            break;
        case SELVA_OBJECT_DOUBLE:
            RedisModule_SaveDouble(io, key->emb_double_value);
            break;
        case SELVA_OBJECT_LONGLONG:
            RedisModule_SaveSigned(io, key->emb_ll_value);
            break;
        case SELVA_OBJECT_STRING:
            rdb_save_object_string(io, key);
            break;
        case SELVA_OBJECT_OBJECT:
            if (!key->value) {
                RedisModule_LogIOError(io, "warning", "OBJECT value missing");
                /* TODO This would create a fatally broken RDB file. */
                break;
            }
            SelvaObjectTypeRDBSave(io, key->value, ptr_save_data);
            break;
        case SELVA_OBJECT_SET:
            rdb_save_object_set(io, key);
            break;
        case SELVA_OBJECT_ARRAY:
            /* TODO Support arrays */
            RedisModule_LogIOError(io, "warning", "Array not supported in RDB");
            break;
        case SELVA_OBJECT_POINTER:
            if (key->ptr_opts && key->ptr_opts->ptr_save) {
                RedisModule_SaveUnsigned(io, key->ptr_opts->ptr_type_id); /* This is used to locate the loader on RDB load. */
                key->ptr_opts->ptr_save(io, key->value, ptr_save_data);
            } else {
                RedisModule_LogIOError(io, "warning", "ptr_save() not given");
                break;
            }
            break;
        default:
            RedisModule_LogIOError(io, "warning", "Unknown type");
        }
    }
}

void _SelvaObjectTypeRDBSave(RedisModuleIO *io, void *value) {
    SelvaObjectTypeRDBSave(io, value, NULL);
}

void SelvaObjectTypeAOFRewrite(RedisModuleIO *aof, RedisModuleString *key __unused, void *value __unused) {
    RedisModule_LogIOError(aof, "warning", "AOF rewrite not supported");
}

void SelvaObjectTypeFree(void *value) {
    struct SelvaObject *obj = (struct SelvaObject *)value;

    SelvaObject_Destroy(obj);
}

static int SelvaObject_OnLoad(RedisModuleCtx *ctx) {
    RedisModuleTypeMethods tm = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = _SelvaObjectTypeRDBLoad,
        .rdb_save = _SelvaObjectTypeRDBSave,
        .aof_rewrite = SelvaObjectTypeAOFRewrite,
        .mem_usage = SelvaObject_MemUsage,
        .free = SelvaObjectTypeFree,
    };

    ObjectType = RedisModule_CreateDataType(ctx, "selva_obj", SELVA_OBJECT_ENCODING_VERSION, &tm);
    if (ObjectType == NULL) {
        return REDISMODULE_ERR;
    }

    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.object.del", SelvaObject_DelCommand, "write", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.exists", SelvaObject_ExistsCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.get", SelvaObject_GetCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
#if 0
        RedisModule_CreateCommand(ctx, "selva.object.getrange", SelvaObject_GetRangeCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.incrby", SelvaObject_IncrbyCommand, "write", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.incrbydouble", SelvaObject_IncrbyDoubleCommand, "write", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.keys", SelvaObject_KeysCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
#endif
        RedisModule_CreateCommand(ctx, "selva.object.len", SelvaObject_LenCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
#if 0
        RedisModule_CreateCommand(ctx, "selva.object.mget", SelvaObject_MgetCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.mset", SelvaObject_MsetCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.scan", SelvaObject_ScanCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
#endif
        RedisModule_CreateCommand(ctx, "selva.object.set", SelvaObject_SetCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
#if 0
        RedisModule_CreateCommand(ctx, "selva.object.setnx", SelvaObject_SetNXCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.strlen", SelvaObject_StrlenCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
#endif
        RedisModule_CreateCommand(ctx, "selva.object.type", SelvaObject_TypeCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        /*RedisModule_CreateCommand(ctx, "selva.object.vals", SelvaObject_ValsCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR*/
        RedisModule_CreateCommand(ctx, "selva.object.getmeta", SelvaObject_GetMetaCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.setmeta", SelvaObject_SetMetaCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(SelvaObject_OnLoad);
