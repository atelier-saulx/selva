/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "endian.h"
#include "jemalloc.h"
#include "linker_set.h"
#include "tree.h"
#include "util/array_field.h"
#include "util/cstrings.h"
#include "util/hll.h"
#include "util/selva_string.h"
#include "util/svector.h"
#include "selva_db.h"
#include "selva_io.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_server.h"
#include "selva_set.h"
#include "selva_trace.h"
#include "selva_object.h"

#define MOD_AL(x, y) ((x) & (y - 1)) /* x % bytes */
#define PAD(size, al) MOD_AL((al - MOD_AL(size, al)), al)
#define ALIGNED_SIZE(size, al) (size + PAD(size, al))

#define SELVA_OBJECT_KEY_MAX            USHRT_MAX /*!< Maximum length of a key including dots and array notation. */
#define SELVA_OBJECT_SIZE_MAX           0x7FFFFFFF /*!< Maximum number of keys in a SelvaObject. */
/**
 * Number of keys embedded into the object.
 * Must be a power of two. This must be relatively small because we are doing
 * a linear search into the embedded keys.
 */
#define NR_EMBEDDED_KEYS                4
/**
 * Maximum length of the name of an embedded key.
 * Must align nicely with the SelvaObject structure. This is ensured by the macros.
 */
#define EMBEDDED_NAME_MAX               ALIGNED_SIZE(9, alignof(struct SelvaObjectKey))
#define EMBEDDED_KEY_SIZE               (sizeof(struct SelvaObjectKey) + EMBEDDED_NAME_MAX)

#define SELVA_OBJECT_FLAG_DYNAMIC       0x01 /*!< Dynamic allocation with SelvaObject_New(). */
#define SELVA_OBJECT_FLAG_STATIC        0x02 /*!< Static allocation, do not free. */

enum SelvaObjectGetKeyFlags {
    SELVA_OBJECT_GETKEY_CREATE        = 0x01, /*!< Create the key and required nested objects if missing. */
    SELVA_OBJECT_GETKEY_DELETE        = 0x02, /*!< Delete the key found. */
    SELVA_OBJECT_GETKEY_PARTIAL       = 0x04, /*!< Return a partial result, the last key found, and the length of the match in key_name_str. */
};

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
        SVector *array; /*!< SELVA_OBJECT_ARRAY */
    };
    char name[0]; /*!< Name of the key. nul terminated. */
};

struct SelvaObject {
    uint32_t obj_size;
    uint16_t emb_res; /*!< Bitmap for reserved embedded fields. */
    uint16_t flags;
    struct SelvaObjectKeys keys_head;
    _Alignas(struct SelvaObjectKey) char emb_keys[NR_EMBEDDED_KEYS * EMBEDDED_KEY_SIZE];
};

/* Change SELVA_OBJECT_BSIZE in selva_object.h if this fails. */
_Static_assert(SELVA_OBJECT_BSIZE == sizeof(struct SelvaObject), "Sizes must match");
/* Change all the code using SelvaObject_Init() if this fails :) */
_Static_assert(alignof(struct SelvaObject) == 8, "SelvaObject align should be well known");

struct so_type_name {
    const char * const name;
    size_t len;
};

SET_DECLARE(selva_objpop, struct SelvaObjectPointerOpts);

SELVA_TRACE_HANDLE(SelvaObject_get_key);

/**
 * Defaults for SELVA_OBJECT_POINTER handling.
 * The default is: Do Nothing.
 */
static const struct SelvaObjectPointerOpts default_ptr_opts = {
    .ptr_type_id = SELVA_OBJECT_POINTER_NOP,
};
SELVA_OBJECT_POINTER_OPTS(default_ptr_opts);

RB_PROTOTYPE_STATIC(SelvaObjectKeys, SelvaObjectKey, _entry, SelvaObject_Compare)
static int get_key(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, unsigned flags, struct SelvaObjectKey **out);
static void replyWithKeyValue(struct selva_server_response_out *resp, struct selva_string *lang, struct SelvaObjectKey *key);
static void replyWithObject(struct selva_server_response_out *resp, struct selva_string *lang, struct SelvaObject *obj, const char *excluded);
static struct SelvaObject *load_object(struct selva_io *io, int encver, int level, void *ptr_load_data);

static int SelvaObject_Compare(const struct SelvaObjectKey *a, const struct SelvaObjectKey *b) {
    /*
     * strcmp() is slightly faster than memcmp() in this case.
     * We could compare the string lengths before doing the comparison but that
     * doesn't really change much as the biggest latency in this function is
     * caused by the fetch from DRAM.
     */
    return strcmp(a->name, b->name);
}

RB_GENERATE_STATIC(SelvaObjectKeys, SelvaObjectKey, _entry, SelvaObject_Compare)

static const struct so_type_name type_names[] = {
    /* type_code                    name_str        name_len */
    [SELVA_OBJECT_NULL] =         { "null",         4 },
    [SELVA_OBJECT_DOUBLE] =       { "double",       6 },
    [SELVA_OBJECT_LONGLONG] =     { "long long",    9 },
    [SELVA_OBJECT_STRING] =       { "string",       6 },
    [SELVA_OBJECT_OBJECT] =       { "object",       6 },
    [SELVA_OBJECT_SET] =          { "selva_set",    9 },
    [SELVA_OBJECT_ARRAY] =        { "array",        5 },
    [SELVA_OBJECT_POINTER] =      { "pointer",      7 },
    [SELVA_OBJECT_HLL] =          { "hll",          3 },
};

static void init_obj(struct SelvaObject *obj) {
    obj->obj_size = 0;
    obj->emb_res = (1 << NR_EMBEDDED_KEYS) - 1;
    RB_INIT(&obj->keys_head);
}

struct SelvaObject *SelvaObject_New(void) {
    struct SelvaObject *obj;

    obj = selva_malloc(sizeof(*obj));
    init_obj(obj);
    obj->flags = SELVA_OBJECT_FLAG_DYNAMIC;

    return obj;
}

struct SelvaObject *SelvaObject_Init(char buf[SELVA_OBJECT_BSIZE]) {
    struct SelvaObject *obj = (struct SelvaObject *)buf;

    /*
     * The given buffer must honor this unknown alignment (probably 8, see the
     * static assert above) or we can't guarantee that it would work properly as
     * a SelvaObject.
     */
    assert(((uintptr_t)(const void *)buf) % alignof(struct SelvaObject) == 0);

    init_obj(obj);
    obj->flags = SELVA_OBJECT_FLAG_STATIC;

    return obj;
}

static struct SelvaObjectPointerOpts *get_ptr_opts(unsigned ptr_type_id) {
    struct SelvaObjectPointerOpts **p;

retry:
    SET_FOREACH(p, selva_objpop) {
        struct SelvaObjectPointerOpts *opts = *p;

        if (opts->ptr_type_id == ptr_type_id) {
            return opts;
        }
    }

    ptr_type_id = 0;
    goto retry;

    unreachable();
    return NULL; /* Never reached. */
}

static int is_valid_array_subtype(enum SelvaObjectType subtype)
{
    switch (subtype) {
    case SELVA_OBJECT_LONGLONG:
    case SELVA_OBJECT_DOUBLE:
    case SELVA_OBJECT_STRING:
    case SELVA_OBJECT_OBJECT:
    case SELVA_OBJECT_POINTER:
    case SELVA_OBJECT_HLL:
        return 1;
    default:
        return 0;
    }
}

/**
 * Init an array on key.
 * The key must be cleared before calling this function.
 */
static void init_object_array(struct SelvaObjectKey *key, enum SelvaObjectType subtype, size_t size) {
    assert(key->type == SELVA_OBJECT_NULL);

    key->type = SELVA_OBJECT_ARRAY;
    key->subtype = subtype;
    key->array = selva_calloc(1, sizeof(SVector));
    SVector_Init(key->array, size, NULL);
}

/**
 * Array value from idx to k.
 * @param k should be typically allocated from the stack.
 * @param arr_key is the array key.
 * @param array is typically key->array.
 * @returns k.
 */
static struct SelvaObjectKey *array_ind_to_key(struct SelvaObjectKey *restrict k, struct SelvaObjectKey *restrict arr_key, ssize_t ary_idx) {
    void *p;

    assert(arr_key->type == SELVA_OBJECT_ARRAY);

    *k = (struct SelvaObjectKey){
        .type = arr_key->subtype,
        .subtype = SELVA_OBJECT_NULL,
        .user_meta = 0, /* TODO What's the meta value for array members. */
        .name_len = 0,
    };

    if (ary_idx < 0) {
        ary_idx = vec_idx_to_abs(arr_key->array, ary_idx);
    }

    /* NULL is fine for an index that doesn't exist. */
    p = SVector_GetIndex(arr_key->array, ary_idx);

    switch (arr_key->subtype) {
        case SELVA_OBJECT_NULL:
            k->value = NULL;
            break;
        case SELVA_OBJECT_DOUBLE:
            memcpy(&k->emb_double_value, &p, sizeof(double));
            break;
        case SELVA_OBJECT_LONGLONG:
            memcpy(&k->emb_ll_value, &p, sizeof(long long));
            break;
        case SELVA_OBJECT_POINTER: /* ptr opts not supported. */
            k->ptr_opts = &default_ptr_opts;
            [[fallthrough]];
        case SELVA_OBJECT_STRING:
        case SELVA_OBJECT_OBJECT:
        case SELVA_OBJECT_SET:
        case SELVA_OBJECT_HLL:
            k->value = p;
            break;
        case SELVA_OBJECT_ARRAY:
            k->array = p;
            break;
    }

    return k;
}

static void clear_object_array(enum SelvaObjectType subtype, SVector *array) {
    switch (subtype) {
    case SELVA_OBJECT_STRING:
        {
            struct SVectorIterator it;

            SVector_ForeachBegin(&it, array);
            do {
                selva_string_free(SVector_Foreach(&it));
            } while (!SVector_Done(&it));
        }
        break;
    case SELVA_OBJECT_OBJECT:
        {
            struct SVectorIterator it;

            SVector_ForeachBegin(&it, array);
            do {
                struct SelvaObject *k;

                k = SVector_Foreach(&it);
                if (k) {
                    SelvaObject_Destroy(k);
                }
            } while (!SVector_Done(&it));
        }
        break;
    case SELVA_OBJECT_POINTER:
    case SELVA_OBJECT_NULL:
    case SELVA_OBJECT_DOUBLE:
    case SELVA_OBJECT_LONGLONG:
        /*
         * NOP
         *
         * We store concrete values so it's enough to just clear the SVector.
         *
         * Pointer arrays don't support cleanup but it would be possible to
         * add support for SelvaObjectPointerOpts.
         */
        break;
    case SELVA_OBJECT_HLL:
        {
            struct SVectorIterator it;

            SVector_ForeachBegin(&it, array);
            do {
                hll_t *s;

                s = SVector_Foreach(&it);
                if (s) {
                    hll_destroy(s);
                }
            } while (!SVector_Done(&it));
        }
        break;
     default:
        SELVA_LOG(SELVA_LOGL_ERR, "Key clear failed: Unsupported array type %s (%d)",
                  SelvaObject_Type2String(subtype, NULL),
                  (int)subtype);
    }

    SVector_Destroy(array);
    selva_free(array);
}

static void clear_key_value(struct SelvaObjectKey *key) {
    switch (key->type) {
    case SELVA_OBJECT_NULL:
    case SELVA_OBJECT_DOUBLE:
    case SELVA_OBJECT_LONGLONG:
        /* NOP */
        break;
    case SELVA_OBJECT_STRING:
        if (key->value) {
            selva_string_free(key->value);
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
        clear_object_array(key->subtype, key->array);
        key->array = NULL;
        break;
    case SELVA_OBJECT_POINTER:
        if (key->ptr_opts && key->ptr_opts->ptr_free) {
            key->ptr_opts->ptr_free(key->value);
        }
        key->value = NULL; /* Not strictly necessary. */
        break;
    case SELVA_OBJECT_HLL:
        hll_destroy(key->value);
        key->value = NULL;
        break;
    default:
        /*
         * In general default shouldn't be used because it may mask out missing
         * type handling but it's acceptable here.
         */
        SELVA_LOG(SELVA_LOGL_ERR, "Unknown object value type (%d)",
                  (int)key->type);
    }

    key->type = SELVA_OBJECT_NULL;
}

/**
 * Get embedded key from index i.
 * Note: There is no bounds checking in this function.
 */
static inline struct SelvaObjectKey *get_emb_key(struct SelvaObject *obj, size_t i) {
    return (struct SelvaObjectKey *)(obj->emb_keys + i * EMBEDDED_KEY_SIZE);
}

/**
 * Allocate and initialize a new key.
 */
static struct SelvaObjectKey *alloc_key(struct SelvaObject *obj, const char *name_str, size_t name_len) {
    const size_t key_size = sizeof(struct SelvaObjectKey) + name_len + 1;
    struct SelvaObjectKey *key;

    if (name_len < EMBEDDED_NAME_MAX && obj->emb_res != 0) {
        int i = __builtin_ffs(obj->emb_res) - 1;

        obj->emb_res &= ~(1 << i); /* Reserve it. */
        key = get_emb_key(obj, i);
    } else {
        key = selva_malloc(key_size);
    }

    memset(key, 0, key_size);
    memcpy(key->name, name_str, name_len);
    key->name_len = (typeof(key->name_len))name_len;

    return key;
}

static void remove_key(struct SelvaObject *obj, struct SelvaObjectKey *key) {
    const intptr_t i = ((intptr_t)key - (intptr_t)obj->emb_keys) / EMBEDDED_KEY_SIZE;

    /* Clear and free the key. */
    RB_REMOVE(SelvaObjectKeys, &obj->keys_head, key);
    obj->obj_size--;
    clear_key_value(key);

    if (i >= 0 && i < NR_EMBEDDED_KEYS) {
        /* The key was allocated from the embedded keys. */
        obj->emb_res |= 1 << i; /* Mark it free. */
    } else {
        /* The key was allocated with selva_malloc. */
        selva_free(key);
    }
}

void SelvaObject_Clear(struct SelvaObject *obj, const char * const exclude[]) {
    struct SelvaObjectKey *next;

    for (struct SelvaObjectKey *key = RB_MIN(SelvaObjectKeys, &obj->keys_head); key != NULL; key = next) {
        int clear = 1;

        next = RB_NEXT(SelvaObjectKeys, &obj->keys_head, key);

        if (exclude) {
            for (const char * const * skip = exclude; *skip != NULL; skip++) {
                if (!strcmp(key->name, *skip)) {
                    clear = 0;
                    break;
                }
            }
        }

        if (clear) {
            remove_key(obj, key);
        }
    }
}

void SelvaObject_Destroy(struct SelvaObject *obj) {
    if (!obj || !obj->flags) {
        return;
    }

    SelvaObject_Clear(obj, NULL);
    memset(obj, 0, sizeof(*obj));
    if (obj->flags & SELVA_OBJECT_FLAG_DYNAMIC) {
        selva_free(obj);
    }
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
                size += selva_string_get_len(key->value) + 1;
            }
            break;
        case SELVA_OBJECT_OBJECT:
            if (key->value) {
                size += SelvaObject_MemUsage(key->value);
            }
            break;
        case SELVA_OBJECT_HLL:
            if (key->value) {
                size += hll_bsize(key->value);
            }
            break;
        default:
            /* 0 */
            break;
        }
    }

    return size;
}

static int insert_new_key(struct SelvaObject *obj, const char *name_str, size_t name_len, struct SelvaObjectKey **key_out) {
    struct SelvaObjectKey *key;

    if (obj->obj_size == SELVA_OBJECT_SIZE_MAX) {
        return SELVA_OBJECT_EOBIG;
    }

    key = alloc_key(obj, name_str, name_len);
#if 0
    if ((char *)key >= obj->emb_keys && (char *)key < obj->emb_keys + sizeof(obj->emb_keys)) {
        SELVA_LOG(SELVA_LOGL_DBG, "Key \"%.*s\" is embedded %zu", (int)name_len, name_str, EMBEDDED_NAME_MAX);
    }
#endif

    obj->obj_size++;
    (void)RB_INSERT(SelvaObjectKeys, &obj->keys_head, key);

    *key_out = key;
    return 0;
}

static int insert_new_obj_into_array(struct SelvaObject *obj, const char *s, size_t slen, ssize_t ary_idx, struct SelvaObject **out) {
    struct SelvaObject *new_obj;
    int err;

    new_obj = SelvaObject_New();
    err = SelvaObject_AssignArrayIndexStr(obj, s, slen, SELVA_OBJECT_OBJECT, ary_idx, new_obj);
    if (err) {
        SelvaObject_Destroy(new_obj);
        return err;
    }

    *out = new_obj;
    return 0;
}

static int get_key_obj(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectGetKeyFlags flags, struct SelvaObjectKey **out) {
    int is_timeseries = 0;
    const char *sep = ".";
    const size_t nr_parts = substring_count(key_name_str, sep, key_name_len) + 1;
    char buf[key_name_len + 1]; /* We assume that the length has been sanity checked at this point. */
    struct SelvaObjectKey *key = NULL;

    memcpy(buf, key_name_str, key_name_len);
    buf[key_name_len] = '\0';

    size_t nr_parts_found = 0;
    char *rest;
    char *s;
    for (s = strtok_r(buf, sep, &rest);
         s != NULL;
         s = strtok_r(NULL, sep, &rest)) {
        size_t slen = strlen(s); /* strtok_r() is safe. */
        size_t new_len = 0;
        ssize_t ary_idx = -1;
        int err;

        const int ary_err = get_array_field_index(s, slen, &ary_idx);
        if (ary_err < 0) {
            return SELVA_EINVAL;
        } else if (ary_err > 0) {
            new_len = ary_err;

            if (ary_idx < 0) {
                const size_t ary_len = SelvaObject_GetArrayLenStr(obj, s, new_len);

                ary_idx = ary_idx_to_abs(ary_len, ary_idx);
            }
        }

        /*
         * Replace the current s if s was an array field.
         */
        char new_s[new_len + 1];
        if (new_len > 0) {
            memcpy(new_s, s, new_len);
            new_s[new_len] = '\0';

            s = new_s;
            slen = new_len;
        }

        key = NULL; /* This needs to be cleared on every iteration. */
        nr_parts_found++;

        if (is_timeseries && s[0] != '_') {
            err = get_key(obj, "_value", 6, 0, &key);
            if (err) {
                return err;
            }

            obj = key->value;
            is_timeseries = 0;
            key = NULL;
        }

        err = get_key(obj, s, slen, 0, &key);
        if (ary_idx >= 0 &&
            (err == SELVA_ENOENT || (err == 0 && (key->type != SELVA_OBJECT_ARRAY && nr_parts > nr_parts_found))) &&
            (flags & SELVA_OBJECT_GETKEY_CREATE)) {
            /*
             * Expected an array.
             * Either the nested object doesn't exist yet or the nested key is not an object,
             * but we are allowed to create one here. Only create the key if it didn't exist.
             * Otherwise we can just reuse the old one.
             */
            if (!key) {
                err = insert_new_key(obj, s, slen, &key);
                if (err) {
                    return err;
                }
            } else {
                /*
                 * Clear the old value.
                 */
                clear_key_value(key);
            }

            init_object_array(key, SELVA_OBJECT_OBJECT, ary_idx + 1);
            err = insert_new_obj_into_array(obj, s, slen, ary_idx, &obj);
            if (err) {
                return err;
            }
        } else if ((err == SELVA_ENOENT || (err == 0 && key->type != SELVA_OBJECT_OBJECT && key->type != SELVA_OBJECT_ARRAY && nr_parts > nr_parts_found)) &&
                   (flags & SELVA_OBJECT_GETKEY_CREATE)) {
            /*
             * Expected a nested object.
             * Either the nested object doesn't exist yet or the nested key is not an object,
             * but we are allowed to create one here. Only create the key if it didn't exist.
             * Otherwise we can just reuse the old one.
             */
            if (!key) {
                err = insert_new_key(obj, s, slen, &key);
                if (err) {
                    return err;
                }
            } else {
                /*
                 * Clear the old value.
                 */
                clear_key_value(key);
            }

            key->type = SELVA_OBJECT_OBJECT;
            key->value = SelvaObject_New();

            obj = key->value;
        } else if (err) {
            /*
             * Error, bail out.
             */
            return err;
        } else /* no error cases: */ if (key->type == SELVA_OBJECT_OBJECT) {
            /*
             * Keep nesting or return an object if this was the last token.
             */

            if (key->user_meta == SELVA_OBJECT_META_SUBTYPE_TIMESERIES) {
                is_timeseries = 1;
            }

            obj = key->value;
        } else if (key->type == SELVA_OBJECT_ARRAY && key->subtype == SELVA_OBJECT_OBJECT && nr_parts > nr_parts_found && ary_idx >= 0) {
            /*
             * Keep nesting or return an object from the array if this was the last token.
             */
            err = SelvaObject_GetArrayIndexAsSelvaObject(obj, s, slen, ary_idx, &obj);
            if ((err == SELVA_ENOENT || !obj) && (flags & SELVA_OBJECT_GETKEY_CREATE)) {
                err = insert_new_obj_into_array(obj, s, slen, ary_idx, &obj);
            }
            if (err) {
                return err;
            }
        } else {
            /*
             * Found the final key or no exact match.
             */
            break;
        }
    }

    if (nr_parts_found < nr_parts &&
        key &&
        (flags & SELVA_OBJECT_GETKEY_PARTIAL) &&
        s) {
        const size_t off = (size_t)(s - buf + 1) + strlen(s);

        *out = key;
        return off;
    }

    /*
     * Check that we found what we were really looking for. Consider the
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
        remove_key(obj, key);
        key = NULL;
    }

    *out = key;
    return 0;
}

static struct SelvaObjectKey *find_key_emb(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    if (key_name_len < EMBEDDED_NAME_MAX) {
        const unsigned k = obj->emb_res;

        for (int i = 0; i < NR_EMBEDDED_KEYS; i++) {
            if ((k & (1 << i)) == 0) {
                struct SelvaObjectKey *key = get_emb_key(obj, i);

                if (key->name_len == key_name_len && !memcmp(key->name, key_name_str, key_name_len)) {
                    return key;
                }
            }
        }
    }

    return NULL;
}

static struct SelvaObjectKey *find_key_rb(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    const size_t key_size = sizeof(struct SelvaObjectKey) + key_name_len + 1;
    _Alignas(struct SelvaObjectKey) char buf[key_size];
    struct SelvaObjectKey *filter = (struct SelvaObjectKey *)buf;

    memset(filter, 0, key_size);
    memcpy(filter->name, key_name_str, key_name_len);
    filter->name_len = key_name_len;

    return RB_FIND(SelvaObjectKeys, &obj->keys_head, filter);
}

static struct SelvaObjectKey *find_key(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    struct SelvaObjectKey *key;

    key = find_key_emb(obj, key_name_str, key_name_len);
    if (!key) {
        /* Otherwise look from the RB tree. */
        key = find_key_rb(obj, key_name_str, key_name_len);
    }

    return key;
}

static int get_key(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectGetKeyFlags flags, struct SelvaObjectKey **out) {
    SELVA_TRACE_BEGIN_AUTO(SelvaObject_get_key);
    ssize_t ary_err;
    struct SelvaObjectKey *key;

    /* Prefetch seems to help just a little bit here. */
    __builtin_prefetch(obj, 0, 0);

    if (key_name_len + 1 > SELVA_OBJECT_KEY_MAX) {
        return SELVA_ENAMETOOLONG;
    }

    /*
     * Note that we generally return whatever was found at the key name
     * ignoring the last index.
     */
    ary_err = get_array_field_index(key_name_str, key_name_len, NULL);
    if (ary_err < 0) {
        return SELVA_EINVAL;
    } else if (ary_err > 0) {
        key_name_len = ary_err;
    }

    if (memmem(key_name_str, key_name_len, ".", 1)) {
        /* Look for a nested object. */
        return get_key_obj(obj, key_name_str, key_name_len, flags, out);
    }

    key = find_key(obj, key_name_str, key_name_len);
    if (!key && (flags & SELVA_OBJECT_GETKEY_CREATE)) {
        int err;

        err = insert_new_key(obj, key_name_str, key_name_len, &key);
        if (err) {
            return err;
        }
    } else if (!key) {
        return SELVA_ENOENT;
    }

    /* FIXME What if there was an index given? */
    if (flags & SELVA_OBJECT_GETKEY_DELETE) {
        remove_key(obj, key);
        key = NULL;
    }

    *out = key;
    return 0;
}

static inline int array_type_match(const struct SelvaObjectKey *key, enum SelvaObjectType subtype) {
    return key->type == SELVA_OBJECT_ARRAY && key->subtype == subtype;
}

static int get_key_array_modify(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, size_t size_hint, struct SelvaObjectKey **key_out) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    if (!array_type_match(key, subtype)) {
        if (!is_valid_array_subtype(subtype)) {
            return SELVA_EINTYPE;
        }

        clear_key_value(key);
        init_object_array(key, subtype, size_hint);
    }

    *key_out = key;
    return 0;
}

static int get_selva_set(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaSetType type, struct SelvaSet **set_out) {
    struct SelvaObjectKey *key;
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

static int get_selva_set_modify(struct SelvaObject *obj, const struct selva_string *key_name, enum SelvaSetType type, struct SelvaSet **set_out) {
    TO_STR(key_name);
    struct SelvaObjectKey *key;
    ssize_t ary_err;
    int err;

    assert(obj);

    /*
     * SelvaSet in array is not supported.
     */
    ary_err = get_array_field_index(key_name_str, key_name_len, NULL);
    if (ary_err < 0) {
        return SELVA_EINVAL;
    } else if (ary_err > 0) {
        return SELVA_ENOTSUP;
    }

    if (!SelvaSet_isValidType(type)) {
        return SELVA_EINTYPE;
    }

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    if (key->type != SELVA_OBJECT_SET) {
        clear_key_value(key);
        SelvaSet_Init(&key->selva_set, type);
        key->type = SELVA_OBJECT_SET;
    } else if (key->selva_set.type != type) {
        return SELVA_EINTYPE;
    }

    *set_out = &key->selva_set;
    return 0;
}

int SelvaObject_DelKeyStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    struct SelvaObjectKey *key;

    assert(obj);

    return get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_DELETE, &key);
}

int SelvaObject_DelKey(struct SelvaObject *obj, const struct selva_string *key_name) {
    TO_STR(key_name);

    return SelvaObject_DelKeyStr(obj, key_name_str, key_name_len);
}

int SelvaObject_ExistsStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    struct SelvaObjectKey *key;
    ssize_t ary_idx, ary_err, ary_len;
    int err;

    assert(obj);

    ary_err = get_array_field_index(key_name_str, key_name_len, &ary_idx);
    if (ary_err < 0) {
        err = SELVA_EINVAL;
    }

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err || ary_err == 0) {
        return err;
    }

    assert(key && key->type == SELVA_OBJECT_ARRAY);

    /* array index given */
    ary_len = SVector_Size(key->array);
    ary_idx = ary_idx_to_abs(ary_len, ary_idx);

    switch (key->subtype) {
    case SELVA_OBJECT_LONGLONG:
    case SELVA_OBJECT_DOUBLE:
        return (ary_idx < ary_len) ? 0 : SELVA_ENOENT;
    case SELVA_OBJECT_STRING:
    case SELVA_OBJECT_OBJECT:
    case SELVA_OBJECT_HLL:
        return SVector_GetIndex(key->array, ary_idx) ? 0 : SELVA_ENOENT;
    default:
        return SELVA_ENOENT;
    }
}

int SelvaObject_Exists(struct SelvaObject *obj, const struct selva_string *key_name) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);

    assert(obj);

    return get_key(obj, key_name_str, key_name_len, 0, &key);
}

int SelvaObject_ExistsTopLevel(struct SelvaObject *obj, const struct selva_string *key_name) {
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

int SelvaObject_GetDouble(struct SelvaObject *obj, const struct selva_string *key_name, double *out) {
    TO_STR(key_name);

    return SelvaObject_GetDoubleStr(obj, key_name_str, key_name_len, out);
}


int SelvaObject_SetDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value) {
    ssize_t ary_err, idx;
    int err;

    assert(obj);

    ary_err = get_array_field_index(key_name_str, key_name_len, &idx);
    if (ary_err < 0) {
        err = SELVA_EINVAL;
    } else if (ary_err > 0) {
        size_t new_len = ary_err;
        void *ptr;

        _Static_assert(sizeof(ptr) == sizeof(value));
        memcpy(&ptr, &value, sizeof(value));
        err = SelvaObject_AssignArrayIndexStr(obj, key_name_str, new_len, SELVA_OBJECT_DOUBLE, idx, ptr);
    } else {
        struct SelvaObjectKey *key;

        err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
        if (!err) {
            clear_key_value(key);
            key->type = SELVA_OBJECT_DOUBLE;
            key->emb_double_value = value;
        }
    }

    return err;
}

int SelvaObject_SetDouble(struct SelvaObject *obj, const struct selva_string *key_name, double value) {
    TO_STR(key_name);

    return SelvaObject_SetDoubleStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_SetDoubleDefaultStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    if (SelvaObject_GetTypeStr(obj, key_name_str, key_name_len) != SELVA_OBJECT_NULL) {
        return SELVA_EEXIST;
    }

    /*
     * Note that SELVA_OBJECT_GETKEY_CREATE makes get_key() to return an object
     * for nested keys if the key didn't exist yet and clears any existing
     * value, that's why we used SelvaObject_GetTypeStr() earlier instead of
     * using key->type here.
     */
    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    clear_key_value(key);
    key->type = SELVA_OBJECT_DOUBLE;
    key->emb_double_value = value;

    return 0;
}

int SelvaObject_SetDoubleDefault(struct SelvaObject *obj, const struct selva_string *key_name, double value) {
    TO_STR(key_name);

    return SelvaObject_SetDoubleDefaultStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_UpdateDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value) {
    const enum SelvaObjectType type = SELVA_OBJECT_DOUBLE;
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    } else if (key->type == type &&
               key->emb_double_value == value) {
        return SELVA_EEXIST;
    }

    clear_key_value(key);
    key->type = type;
    key->emb_double_value = value;

    return 0;
}

int SelvaObject_UpdateDouble(struct SelvaObject *obj, const struct selva_string *key_name, double value) {
    TO_STR(key_name);

    return SelvaObject_UpdateDoubleStr(obj, key_name_str, key_name_len, value);
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

int SelvaObject_GetLongLong(struct SelvaObject *obj, const struct selva_string *key_name, long long *out) {
    TO_STR(key_name);

    return SelvaObject_GetLongLongStr(obj, key_name_str, key_name_len, out);
}

int SelvaObject_SetLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value) {
    ssize_t ary_err, idx;
    int err;

    assert(obj);

    ary_err = get_array_field_index(key_name_str, key_name_len, &idx);
    if (ary_err < 0) {
        err = SELVA_EINVAL;
    } else if (ary_err > 0) {
        size_t new_len = ary_err;
        union {
            long long ll;
            void *p;
        } v = {
            .ll = value,
        };

        err = SelvaObject_AssignArrayIndexStr(obj, key_name_str, new_len, SELVA_OBJECT_LONGLONG, idx, v.p);
    } else {
        struct SelvaObjectKey *key;

        err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
        if (!err) {
            clear_key_value(key);
            key->type = SELVA_OBJECT_LONGLONG;
            key->emb_ll_value = value;
        }
    }

    return err;
}

int SelvaObject_SetLongLong(struct SelvaObject *obj, const struct selva_string *key_name, long long value) {
    TO_STR(key_name);

    return SelvaObject_SetLongLongStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_SetLongLongDefaultStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    if (SelvaObject_GetTypeStr(obj, key_name_str, key_name_len) != SELVA_OBJECT_NULL) {
        return SELVA_EEXIST;
    }

    /*
     * Note that SELVA_OBJECT_GETKEY_CREATE makes get_key() to return an object
     * for nested keys if the key didn't exist yet and clears any existing
     * value, that's why we used SelvaObject_GetTypeStr() earlier instead of
     * using key->type here.
     */
    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    clear_key_value(key);
    key->type = SELVA_OBJECT_LONGLONG;
    key->emb_ll_value = value;

    return 0;
}

int SelvaObject_SetLongLongDefault(struct SelvaObject *obj, const struct selva_string *key_name, long long value) {
    TO_STR(key_name);

    return SelvaObject_SetLongLongDefaultStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_UpdateLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value) {
    const enum SelvaObjectType type = SELVA_OBJECT_LONGLONG;
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    } else if (key->type == type &&
               key->emb_ll_value == value) {
        return SELVA_EEXIST;
    }

    clear_key_value(key);
    key->type = type;
    key->emb_ll_value = value;

    return 0;
}

int SelvaObject_UpdateLongLong(struct SelvaObject *obj, const struct selva_string *key_name, long long value) {
    TO_STR(key_name);

    return SelvaObject_UpdateLongLongStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_GetStringStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct selva_string **out) {
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

int SelvaObject_GetString(struct SelvaObject *obj, const struct selva_string *key_name, struct selva_string **out) {
    TO_STR(key_name);

    return SelvaObject_GetStringStr(obj, key_name_str, key_name_len, out);
}


int SelvaObject_SetStringStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct selva_string *value) {
    ssize_t ary_err, idx;
    int err;

    assert(obj);

    ary_err = get_array_field_index(key_name_str, key_name_len, &idx);
    if (ary_err < 0) {
        err = SELVA_EINVAL;
    } else if (ary_err > 0) {
        size_t new_len = ary_err;

        err = SelvaObject_AssignArrayIndexStr(obj, key_name_str, new_len, SELVA_OBJECT_STRING, idx, value);
    } else {
        struct SelvaObjectKey *key;

        err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
        if (!err) {
            clear_key_value(key);
            key->type = SELVA_OBJECT_STRING;
            key->value = value;
        }
    }

    return err;
}

int SelvaObject_SetString(struct SelvaObject *obj, const struct selva_string *key_name, struct selva_string *value) {
    TO_STR(key_name);

    return SelvaObject_SetStringStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_IncrementDoubleStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double default_value, double incr, double *prev) {
    ssize_t ary_err, ary_idx;

    assert(obj);

    ary_err = get_array_field_index(key_name_str, key_name_len, &ary_idx);
    if (ary_err < 0) {
        return SELVA_EINVAL;
    } else if (ary_err > 0) {
        struct SelvaObjectKey *key;
        void *ptr;
        double d;
        int err;

        err = get_key_array_modify(obj, key_name_str, key_name_len, SELVA_OBJECT_DOUBLE, 1, &key);
        if (err) {
            return err;
        }

        ary_idx = vec_idx_to_abs(key->array, ary_idx);
        ptr = SVector_GetIndex(key->array, ary_idx);
        if (ptr) {
            memcpy(&d, &ptr, sizeof(d));
            _Static_assert(sizeof(d) == sizeof(ptr));
        } else {
            d = 0.0;
        }

        if (prev) {
            *prev = d;
        }

        /* FIXME Default value is handled incorrectly in the array case. */
        d += incr;
        memcpy(&ptr, &d, sizeof(d));
        SVector_SetIndex(key->array, ary_idx, ptr);
        if (err) {
            return err;
        }
    } else {
        struct SelvaObjectKey *key;
        int err;

        err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
        if (err) {
            return err;
        }

        if (key->type == SELVA_OBJECT_DOUBLE) {
            if (prev) {
                *prev = key->emb_double_value;
            }
            key->emb_double_value += incr;
        } else {
            clear_key_value(key);
            key->type = SELVA_OBJECT_DOUBLE;
            key->emb_double_value = default_value;
            if (prev) {
                *prev = 0.0;
            }
        }
    }

    return 0;
}

int SelvaObject_IncrementDouble(struct SelvaObject *obj, const struct selva_string *key_name, double default_value, double incr, double *prev) {
    TO_STR(key_name);

    return SelvaObject_IncrementDoubleStr(obj, key_name_str, key_name_len, default_value, incr, prev);
}

int SelvaObject_IncrementLongLongStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long default_value, long long incr, long long *prev) {
    ssize_t ary_err, ary_idx;

    assert(obj);

    ary_err = get_array_field_index(key_name_str, key_name_len, &ary_idx);
    if (ary_err < 0) {
        return SELVA_EINVAL;
    } else if (ary_err > 0) {
        struct SelvaObjectKey *key;
        void *ptr;
        long long ll;
        int err;

        err = get_key_array_modify(obj, key_name_str, key_name_len, SELVA_OBJECT_LONGLONG, 1, &key);
        if (err) {
            return err;
        }

        ary_idx = vec_idx_to_abs(key->array, ary_idx);
        ptr = SVector_GetIndex(key->array, ary_idx);
        memcpy(&ll, &ptr, sizeof(ll));
        _Static_assert(sizeof(ll) == sizeof(ptr));

        if (prev) {
            *prev = ll;
        }

        /* FIXME Default value is handled incorrectly in the array case. */
        ll += incr;
        memcpy(&ptr, &ll, sizeof(ll));
        SVector_SetIndex(key->array, ary_idx, (void *)ll);
        if (err) {
            return err;
        }
    } else {
        struct SelvaObjectKey *key;
        int err;

        err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
        if (err) {
            return err;
        }

        if (key->type == SELVA_OBJECT_LONGLONG) {
            if (prev) {
                *prev = key->emb_ll_value;
            }
            key->emb_ll_value += incr;
        } else {
            clear_key_value(key);
            key->type = SELVA_OBJECT_LONGLONG;
            key->emb_ll_value = default_value;
            if (prev) {
                *prev = 0;
            }
        }
    }

    return 0;
}

int SelvaObject_IncrementLongLong(struct SelvaObject *obj, const struct selva_string *key_name, long long default_value, long long incr, long long *prev) {
    TO_STR(key_name);

    return SelvaObject_IncrementLongLongStr(obj, key_name_str, key_name_len, default_value, incr, prev);
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

int SelvaObject_GetObject(struct SelvaObject *obj, const struct selva_string *key_name, struct SelvaObject **out) {
    TO_STR(key_name);

    return SelvaObject_GetObjectStr(obj, key_name_str, key_name_len, out);
}

int SelvaObject_SetObjectStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct SelvaObject *value) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    clear_key_value(key);
    key->type = SELVA_OBJECT_OBJECT;
    key->value = value;

    return 0;
}

int SelvaObject_SetObject(struct SelvaObject *obj, const struct selva_string *key_name, struct SelvaObject *value) {
    TO_STR(key_name);

    return SelvaObject_SetObjectStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_AddHllStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, const void *el, size_t el_size) {
    const enum SelvaObjectType type = SELVA_OBJECT_HLL;
    ssize_t ary_err, ary_idx;
    int err;

    assert(obj);

    ary_err = get_array_field_index(key_name_str, key_name_len, &ary_idx);
    if (ary_err < 0) {
        err = SELVA_EINVAL;
    } else if (ary_err > 0) {
        struct SelvaObjectKey *key;
        hll_t *hll_orig;
        hll_t *hll;
        int err;

        err = get_key_array_modify(obj, key_name_str, key_name_len, type, 1, &key);
        if (err) {
            return err;
        }

        ary_idx = vec_idx_to_abs(key->array, ary_idx);
        hll_orig = hll = SVector_GetIndex(key->array, ary_idx);
        if (!hll) {
            hll = hll_create();
            SVector_SetIndex(key->array, ary_idx, hll);
            hll_orig = hll;
        }

        if (hll_add(&hll, el, el_size) < 0) {
            /* RFE Can hll change even when the function fails? */
            return SELVA_EINVAL; /* TODO error? */
        }

        /* RFE would capturing the return value of hll_add() help us here. */
        if (hll_orig != hll) {
            SVector_SetIndex(key->array, ary_idx, hll);
        }
    } else {
        struct SelvaObjectKey *key;

        err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
        if (err) {
            return err;
        } else if (key->type != type) {
            clear_key_value(key);
            key->type = type;
            key->value = hll_create();
        }

        if (hll_add(&key->value, el, el_size) < 0) {
            return SELVA_EINVAL; /* TODO error? */
        }
    }

    return 0;
}

int SelvaObject_AddHll(struct SelvaObject *obj, const struct selva_string *key_name, const void *el, size_t el_size) {
    TO_STR(key_name);

    return SelvaObject_AddHllStr(obj, key_name_str, key_name_len, el, el_size);
}

int SelvaObject_AddDoubleSet(struct SelvaObject *obj, const struct selva_string *key_name, double value) {
    struct SelvaSet *selva_set;
    int err;

    err = get_selva_set_modify(obj, key_name, SELVA_SET_TYPE_DOUBLE, &selva_set);
    if (err) {
        return err;
    }

    return SelvaSet_Add(selva_set, value);
}

int SelvaObject_AddLongLongSet(struct SelvaObject *obj, const struct selva_string *key_name, long long value) {
    struct SelvaSet *selva_set;
    int err;

    err = get_selva_set_modify(obj, key_name, SELVA_SET_TYPE_LONGLONG, &selva_set);
    if (err) {
        return err;
    }

    return SelvaSet_Add(selva_set, value);
}

int SelvaObject_AddStringSet(struct SelvaObject *obj, const struct selva_string *key_name, struct selva_string *value) {
    struct SelvaSet *selva_set;
    int err;

    err = get_selva_set_modify(obj, key_name, SELVA_SET_TYPE_STRING, &selva_set);
    if (err) {
        return err;
    }

    return SelvaSet_Add(selva_set, value);
}

int SelvaObject_RemDoubleSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, double value) {
    struct SelvaSet *selva_set;
    struct SelvaSetElement *el;
    int err;

    err = get_selva_set(obj, key_name_str, key_name_len, SELVA_SET_TYPE_DOUBLE, &selva_set);
    if (err) {
        return err;
    }

    el = SelvaSet_Remove(selva_set, value);
    if (!el) {
        return SELVA_EINVAL;
    }
    SelvaSet_DestroyElement(el);

    return 0;
}

int SelvaObject_RemDoubleSet(struct SelvaObject *obj, const struct selva_string *key_name, double value) {
    TO_STR(key_name);

    return SelvaObject_RemDoubleSetStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_RemLongLongSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, long long value) {
    struct SelvaSet *selva_set;
    struct SelvaSetElement *el;
    int err;

    err = get_selva_set(obj, key_name_str, key_name_len, SELVA_SET_TYPE_LONGLONG, &selva_set);
    if (err) {
        return err;
    }

    el = SelvaSet_Remove(selva_set, value);
    if (!el) {
        return SELVA_EINVAL;
    }
    SelvaSet_DestroyElement(el);

    return 0;
}

int SelvaObject_RemLongLongSet(struct SelvaObject *obj, const struct selva_string *key_name, long long value) {
    TO_STR(key_name);

    return SelvaObject_RemLongLongSetStr(obj, key_name_str, key_name_len, value);
}

int SelvaObject_RemStringSetStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct selva_string *value) {
    struct SelvaSet *selva_set;
    struct SelvaSetElement *el;
    int err;

    err = get_selva_set(obj, key_name_str, key_name_len, SELVA_SET_TYPE_STRING, &selva_set);
    if (err) {
        return err;
    }

    el = SelvaSet_Remove(selva_set, value);
    if (!el) {
        return SELVA_EINVAL;
    }
    selva_string_free(el->value_string);
    SelvaSet_DestroyElement(el);

    return 0;
}

int SelvaObject_RemStringSet(struct SelvaObject *obj, const struct selva_string *key_name, struct selva_string *value) {
    TO_STR(key_name);

    return SelvaObject_RemStringSetStr(obj, key_name_str, key_name_len, value);
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

struct SelvaSet *SelvaObject_GetSet(struct SelvaObject *obj, const struct selva_string *key_name) {
    TO_STR(key_name);

    return SelvaObject_GetSetStr(obj, key_name_str, key_name_len);
}

static int SelvaObject_GetArrayIndex(
        struct SelvaObject *obj,
        const char *key_name_str,
        size_t key_name_len,
        size_t idx,
        enum SelvaObjectType subtype,
        void **out) {
    SVector *array;
    enum SelvaObjectType array_type;
    int err = SelvaObject_GetArrayStr(obj, key_name_str, key_name_len, &array_type, &array);

    if (err) {
        return err;
    }

    if (array_type != subtype) {
        /* Handle type mismatch. */
        return SELVA_EINTYPE;
    }

    void *res = SVector_GetIndex(array, idx);
    if (!res) {
        return SELVA_ENOENT;
    }

    *out = res;
    return 0;
}

int SelvaObject_GetArrayIndexAsRmsStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, size_t idx, struct selva_string **out) {
    return SelvaObject_GetArrayIndex(obj, key_name_str, key_name_len, idx, SELVA_OBJECT_STRING, (void **)out);
}

int SelvaObject_GetArrayIndexAsSelvaObject(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, size_t idx, struct SelvaObject **out) {
    void *p;
    int err;

    err = SelvaObject_GetArrayIndex(obj, key_name_str, key_name_len, idx, SELVA_OBJECT_OBJECT, &p);

    if (!err) {
        *out = p;
    }
    return err;
}

int SelvaObject_GetArrayIndexAsLongLong(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, size_t idx, long long *out) {
    void *lptr;
    int err;

    err = SelvaObject_GetArrayIndex(obj, key_name_str, key_name_len, idx, SELVA_OBJECT_LONGLONG, &lptr);
    if (!err) {
        long long l;

        memcpy(&l, &lptr, sizeof(long long));
        *out = l;
    }
    return err;
}

int SelvaObject_GetArrayIndexAsDouble(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, size_t idx, double *out) {
    void *dptr;
    int err;

    err = SelvaObject_GetArrayIndex(obj, key_name_str, key_name_len, idx, SELVA_OBJECT_DOUBLE, &dptr);
    if (!err) {
        double d;

        memcpy(&d, &dptr, sizeof(double));
        *out = d;
    }
    return err;
}

int SelvaObject_InsertArrayStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, void *p) {
    struct SelvaObjectKey *key;
    int err;

    err = get_key_array_modify(obj, key_name_str, key_name_len, subtype, 1, &key);
    if (!err) {
        SVector_Insert(key->array, p);
    }

    return err;
}

int SelvaObject_InsertArray(struct SelvaObject *obj, const struct selva_string *key_name, enum SelvaObjectType subtype, void *p) {
    TO_STR(key_name);

    return SelvaObject_InsertArrayStr(obj, key_name_str, key_name_len, subtype, p);
}

int SelvaObject_AssignArrayIndexStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, ssize_t idx, void *p) {
    const size_t size_hint = idx >= 0 ? idx + 1 : 1;
    struct SelvaObjectKey *key;
    int err;

    err = get_key_array_modify(obj, key_name_str, key_name_len, subtype, size_hint, &key);
    if (!err) {
        SVector_SetIndex(key->array, vec_idx_to_abs(key->array, idx), p);
    }

    return err;
}

int SelvaObject_AssignArrayIndex(struct SelvaObject *obj, const struct selva_string *key_name, enum SelvaObjectType subtype, ssize_t idx, void *p) {
    TO_STR(key_name);

    return SelvaObject_AssignArrayIndexStr(obj, key_name_str, key_name_len, subtype, idx, p);
}

int SelvaObject_InsertArrayIndexStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, enum SelvaObjectType subtype, ssize_t idx, void *p) {
    const size_t size_hint = idx >= 0 ? idx + 1 : 1;
    struct SelvaObjectKey *key;
    int err;

    err = get_key_array_modify(obj, key_name_str, key_name_len, subtype, size_hint, &key);
    if (!err) {
        SVector_InsertIndex(key->array, vec_idx_to_abs(key->array, idx), p);
    }

    return err;
}

int SelvaObject_InsertArrayIndex(struct SelvaObject *obj, const struct selva_string *key_name, enum SelvaObjectType subtype, ssize_t idx, void *p) {
    TO_STR(key_name);

    return SelvaObject_InsertArrayIndexStr(obj, key_name_str, key_name_len, subtype, idx, p);
}

int SelvaObject_RemoveArrayIndexStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, ssize_t idx) {
    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    if (key->type != SELVA_OBJECT_ARRAY) {
        return SELVA_EINVAL;
    }

    size_t i = vec_idx_to_abs(key->array, idx);
    if (SVector_Size(key->array) < i) {
        return SELVA_EINVAL;
    }

    SVector_RemoveIndex(key->array, i);

    return 0;
}

int SelvaObject_RemoveArrayIndex(struct SelvaObject *obj, const struct selva_string *key_name, ssize_t idx) {
    TO_STR(key_name);

    return SelvaObject_RemoveArrayIndexStr(obj, key_name_str, key_name_len, idx);
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
        *out_p = key->array;
    }

    return 0;
}

int SelvaObject_GetArray(struct SelvaObject *obj, const struct selva_string *key_name, enum SelvaObjectType *out_subtype, SVector **out_p) {
    TO_STR(key_name);

    return SelvaObject_GetArrayStr(obj, key_name_str, key_name_len, out_subtype, out_p);
}

size_t SelvaObject_GetArrayLenStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    SVector *vec;

    if (SelvaObject_GetArrayStr(obj, key_name_str, key_name_len, NULL, &vec)) {
        return 0;
    }

    return SVector_Size(vec);
}

size_t SelvaObject_GetArrayLen(struct SelvaObject *obj, const struct selva_string *key_name) {
    TO_STR(key_name);

    return SelvaObject_GetArrayLenStr(obj, key_name_str, key_name_len);
}

int SelvaObject_SetPointerStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void *p, const struct SelvaObjectPointerOpts *opts) {
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

    clear_key_value(key);
    key->type = SELVA_OBJECT_POINTER;
    key->value = p;
    key->ptr_opts = opts;

    return 0;
}

int SelvaObject_SetPointer(struct SelvaObject *obj, const struct selva_string *key_name, void *p, const struct SelvaObjectPointerOpts *opts) {
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

int SelvaObject_GetPointer(struct SelvaObject *obj, const struct selva_string *key_name, void **out_p) {
    TO_STR(key_name);

    return SelvaObject_GetPointerStr(obj, key_name_str, key_name_len, out_p);
}

int SelvaObject_GetPointerPartialMatchStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, void **out_p) {

    struct SelvaObjectKey *key;
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_PARTIAL, &key);
    if (err < 0) {
        return err;
    }
    const int off = err;

    if (key->type != SELVA_OBJECT_POINTER) {
        return SELVA_EINTYPE;
    }

    if (out_p) {
        *out_p = key->value;
    }

    return off;
}

static void get_any_string(struct SelvaObjectKey *key, struct selva_string *lang, struct SelvaObjectAny *res) {
    TO_STR(lang);

    if (key->user_meta == SELVA_OBJECT_META_SUBTYPE_TEXT && lang_len > 0) {
        char buf[lang_len + 1];
        memcpy(buf, lang_str, lang_len + 1);
        const char *sep = "\n";
        char *rest;

        for (const char *s = strtok_r(buf, sep, &rest);
             s != NULL;
             s = strtok_r(NULL, sep, &rest)) {
            const size_t slen = strlen(s);
            struct SelvaObjectKey *text_key;
            int err = get_key(key->value, s, slen, 0, &text_key);

            /* Ignore errors on purpose. */
            if (!err && text_key->type == SELVA_OBJECT_STRING) {
                res->type = text_key->type;
                res->subtype = text_key->subtype;
                res->user_meta = SELVA_OBJECT_META_SUBTYPE_TEXT;
                memset(res->str_lang, '\0', sizeof(res->str_lang));
                memcpy(res->str_lang, s, min(slen, LANG_MAX));
                res->str = text_key->value;

                break;
            }
        }
    }
}

/**
 * @param[in] lang optional lang for strings.
 * @param[in] key is the key.
 * @param[out] res is the any result.
 */
static void key_to_any(struct selva_string *lang, struct SelvaObjectKey *key, struct SelvaObjectAny *res) {
    res->type = key->type;
    res->subtype = key->subtype;
    res->user_meta = key->user_meta;

    switch (key->type) {
    case SELVA_OBJECT_NULL:
        res->p = NULL;
        break;
    case SELVA_OBJECT_DOUBLE:
        res->d = key->emb_double_value;
        break;
    case SELVA_OBJECT_LONGLONG:
        res->ll = key->emb_ll_value;
        break;
    case SELVA_OBJECT_STRING:
        res->str = key->value;
        /*
         * It's likely that the caller will want to access the string and
         * therefore it might be useful to prefetch it.
         */
        __builtin_prefetch(res->str, 0, 0);
        break;
    case SELVA_OBJECT_OBJECT:
        res->obj = key->value;

        if (key->user_meta == SELVA_OBJECT_META_SUBTYPE_TEXT && lang) {
            get_any_string(key, lang, res);
        }
        break;
    case SELVA_OBJECT_SET:
        res->set = &key->selva_set;
        break;
    case SELVA_OBJECT_ARRAY:
        res->array = key->array;
        break;
    case SELVA_OBJECT_POINTER:
        res->p = key->value;
        break;
    case SELVA_OBJECT_HLL:
        res->ll = hll_count(key->value);
        break;
    }
}

int SelvaObject_GetAnyLangStr(struct SelvaObject *obj, struct selva_string *lang, const char *key_name_str, size_t key_name_len, struct SelvaObjectAny *res) {
    struct SelvaObjectKey *key;
    ssize_t ary_err, ary_idx;
    int err;

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    ary_err = get_array_field_index(key_name_str, key_name_len, &ary_idx);
    if (ary_err < 0) {
        return SELVA_EINVAL;
    } else if (ary_err > 0) {
        struct SelvaObjectKey k;

        if (key->type != SELVA_OBJECT_ARRAY) {
            /*
             * The error could be SELVA_ENOTSUP too but perhaps it's more appropriate
             * to tell the caller that such value doesn't exist. Also many existing
             * callers only expect SELVA_ENOENT.
             */
            return SELVA_ENOENT;
        }


        array_ind_to_key(&k, key, ary_idx);
        k.user_meta = key->user_meta; /* RFE Should array_ind_to_key() do this? */
        key_to_any(lang, &k, res);
    } else {
        key_to_any(lang, key, res);
    }

    return 0;
}

int SelvaObject_GetAnyLang(struct SelvaObject *obj, struct selva_string *lang, const struct selva_string *key_name, struct SelvaObjectAny *res) {
    TO_STR(key_name);

    return SelvaObject_GetAnyLangStr(obj, lang, key_name_str, key_name_len, res);
}

int SelvaObject_GetAnyStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, struct SelvaObjectAny *res) {
    return SelvaObject_GetAnyLangStr(obj, NULL, key_name_str, key_name_len, res);
}

int SelvaObject_GetAny(struct SelvaObject *obj, const struct selva_string *key_name, struct SelvaObjectAny *res) {
    TO_STR(key_name);

    return SelvaObject_GetAnyLangStr(obj, NULL, key_name_str, key_name_len, res);
}

enum SelvaObjectType SelvaObject_GetTypeStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    struct SelvaObjectKey *key;
    int err;

    err = get_key(obj, key_name_str, key_name_len, 0, &key);

    return (err) ? SELVA_OBJECT_NULL : key->type;
}

enum SelvaObjectType SelvaObject_GetType(struct SelvaObject *obj, const struct selva_string *key_name) {
    TO_STR(key_name);

    if (unlikely(!key_name_str)) {
        return SELVA_OBJECT_NULL;
    }

    return SelvaObject_GetTypeStr(obj, key_name_str, key_name_len);
}

/**
 * Determine the length of a SelvaObjectKey.
 * @param ary_idx =0 no index; <0 backwards index (-1 last etc); >0 index + 1.
 */
static ssize_t SelvaObjectKey_Len(struct SelvaObjectKey *key, ssize_t ary_idx) {
    if (key->type != SELVA_OBJECT_ARRAY && ary_idx) {
        return SELVA_ENOTSUP;
    }

    switch (key->type) {
    case SELVA_OBJECT_NULL:
        return 0;
    case SELVA_OBJECT_DOUBLE:
    case SELVA_OBJECT_LONGLONG:
        /* Obviously this is more than one byte but the user doesn't need to care about that. */
        return 1;
    case SELVA_OBJECT_STRING:
        /*
         * Now we don't exactly know if the value is some utf8 text string
         * or a binary buffer Heck, we don't even know what the caller
         * expects. So, we'll just return the byte size.
         */
        if (!key->value) {
            return 0;
        } else {
            return selva_string_get_len(key->value);
        }
    case SELVA_OBJECT_OBJECT:
        if (key->value) {
            const struct SelvaObject *obj2 = (const struct SelvaObject *)key->value;

            return obj2->obj_size;
        } else {
            return 0;
        }
    case SELVA_OBJECT_SET:
        return (ssize_t)key->selva_set.size;
    case SELVA_OBJECT_ARRAY:
        if (ary_idx) {
            struct SelvaObjectKey k;

            return SelvaObjectKey_Len(array_ind_to_key(&k, key, ary_idx > 0 ? ary_idx - 1 : ary_idx), 0);
        } else {
            return SVector_Size(key->array);
        }
    case SELVA_OBJECT_POINTER:
        if (key->ptr_opts && key->ptr_opts->ptr_len) {
            return key->ptr_opts->ptr_len(key->value);
        } else {
            return 1;
        }
    case SELVA_OBJECT_HLL:
        return hll_count(key->value);
    }

    return SELVA_EINTYPE;
}

ssize_t SelvaObject_LenStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    struct SelvaObjectKey *key;
    ssize_t ary_err, ary_idx;
    int err;

    if (!key_name_str || key_name_len == 0) {
        return obj->obj_size;
    }

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    ary_err = get_array_field_index(key_name_str, key_name_len, &ary_idx);
    if (ary_err < 0) {
        return SELVA_EINVAL;
    } else if (ary_err > 0) {
        if (ary_idx >= 0) {
            ary_idx++;
        }
    } else {
        ary_idx = 0;
    }

    return SelvaObjectKey_Len(key, ary_idx);
}

ssize_t SelvaObject_Len(struct SelvaObject *obj, const struct selva_string *key_name) {
    if (!key_name) {
        return obj->obj_size;
    }

    TO_STR(key_name);

    return SelvaObject_LenStr(obj, key_name_str, key_name_len);
}

const char *SelvaObject_Type2String(enum SelvaObjectType type, size_t *len) {
    if ((size_t)type < num_elem(type_names)) {
        const struct so_type_name *tn = &type_names[type];

        if (len) {
            *len = tn->len;
        }
        return tn->name;
    }

    return NULL;
}

int SelvaObject_GetUserMetaStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, SelvaObjectMeta_t *meta) {
    int err;
    struct SelvaObjectKey *key;

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (!err) {
        *meta = key->user_meta;
    }

    return err;
}

int SelvaObject_GetUserMeta(struct SelvaObject *obj, const struct selva_string *key_name, SelvaObjectMeta_t *meta) {
    TO_STR(key_name);

    return SelvaObject_GetUserMetaStr(obj, key_name_str, key_name_len, meta);
}

int SelvaObject_SetUserMetaStr(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, SelvaObjectMeta_t meta, SelvaObjectMeta_t *old_meta) {
    int err;
    struct SelvaObjectKey *key;

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    if (old_meta) {
        *old_meta = key->user_meta;
    }
    key->user_meta = meta;
    return 0;
}

int SelvaObject_SetUserMeta(struct SelvaObject *obj, const struct selva_string *key_name, SelvaObjectMeta_t meta, SelvaObjectMeta_t *old_meta) {
    TO_STR(key_name);

    return SelvaObject_SetUserMetaStr(obj, key_name_str, key_name_len, meta, old_meta);
}

void *SelvaObject_ForeachBegin(struct SelvaObject *obj) {
    return RB_MIN(SelvaObjectKeys, &obj->keys_head);
}

const char *SelvaObject_ForeachKey(const struct SelvaObject *obj, void **iterator) {
    struct SelvaObjectKey *key = *iterator;
    (void)obj; /* This makes the compiler think we are actually using obj. */

    if (!key) {
        return NULL;
    }

    *iterator = RB_NEXT(SelvaObjectKeys, &obj->keys_head, key);

    return key->name;
}

void *SelvaObject_ForeachValue(const struct SelvaObject *obj, void **iterator, const char **name_out, enum SelvaObjectType type) {
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
        return key->array;
    case SELVA_OBJECT_HLL:
        /* FIXME */
        break;
    }

    return NULL;
}

void *SelvaObject_ForeachValueType(
        const struct SelvaObject *obj,
        void **iterator,
        const char **name_out,
        enum SelvaObjectType *type_out) {
    struct SelvaObjectKey *key;
    (void)obj; /* This makes the compiler think we are actually using obj. */

    key = *iterator;
    if (!key) {
        return NULL;
    }

    *iterator = RB_NEXT(SelvaObjectKeys, &obj->keys_head, key);

    if (name_out) {
        *name_out = key->name;
    }
    *type_out = key->type;

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
        return key->array;
    case SELVA_OBJECT_HLL:
        /* FIXME */
        break;
    }

    return NULL;
}

static void replyWithSelvaString(struct selva_server_response_out *resp, struct selva_string *s) {
    if (s) {
        selva_send_string(resp, s);
    } else {
        selva_send_null(resp);
    }
}

static void replyWithSelvaSet(struct selva_server_response_out *resp, struct SelvaSet *set) {
    struct SelvaSetElement *el;
    size_t n = 0;

    selva_send_array(resp, -1);

    if (set->type == SELVA_SET_TYPE_STRING) {
        SELVA_SET_STRING_FOREACH(el, set) {
            selva_send_string(resp, el->value_string);
            n++;
        }
    } else if (set->type == SELVA_SET_TYPE_DOUBLE) {
        SELVA_SET_DOUBLE_FOREACH(el, set) {
            selva_send_double(resp, el->value_d);
            n++;
        }
    } else if (set->type == SELVA_SET_TYPE_LONGLONG) {
        SELVA_SET_LONGLONG_FOREACH(el, set) {
            selva_send_ll(resp, el->value_ll);
            n++;
        }
    }

    selva_send_array_end(resp);
}

static void replyWithHll(struct selva_server_response_out *resp, hll_t *hll)
{
    if (hll) {
        selva_send_ll(resp, hll_count(hll));
    } else {
        selva_send_null(resp);
    }
}

static void replyWithArray(struct selva_server_response_out *resp, struct selva_string *lang, enum SelvaObjectType subtype, const SVector *array) {
    struct SVectorIterator it;

    switch (subtype) {
    case SELVA_OBJECT_DOUBLE:
        selva_send_array_embed(resp, SELVA_SEND_ARRAY_EMBED_DOUBLE, SVector_Size(array));

        SVector_ForeachBegin(&it, array);
        do {
            void *pd;
            char buf[sizeof(double)];

            pd = SVector_Foreach(&it);

            if (!pd) {
                htoledouble(buf, 0.0);
            } else {
                double d;

                memcpy(&d, &pd, sizeof(double));
                htoledouble(buf, d);
            }

            selva_send_raw(resp, buf, sizeof(buf));
        } while (!SVector_Done(&it));

        break;
    case SELVA_OBJECT_LONGLONG:
        selva_send_array_embed(resp, SELVA_SEND_ARRAY_EMBED_LONGLONG, SVector_Size(array));

        SVector_ForeachBegin(&it, array);
        do {
            void *p;
            int64_t v;

            p = SVector_Foreach(&it);
            v = htole64((long long)p);
            selva_send_raw(resp, &v, sizeof(v));
        } while (!SVector_Done(&it));

        break;
    case SELVA_OBJECT_STRING:
        selva_send_array(resp, SVector_Size(array));

        SVector_ForeachBegin(&it, array);
        do {
            replyWithSelvaString(resp, SVector_Foreach(&it));
        } while (!SVector_Done(&it));
        break;
    case SELVA_OBJECT_OBJECT:
        selva_send_array(resp, SVector_Size(array));

        SVector_ForeachBegin(&it, array);
        do {
            replyWithObject(resp, lang, SVector_Foreach(&it), NULL);
        } while (!SVector_Done(&it));
        break;
    case SELVA_OBJECT_HLL:
        selva_send_array(resp, SVector_Size(array));

        SVector_ForeachBegin(&it, array);
        do {
            replyWithHll(resp, SVector_Foreach(&it));
        } while (!SVector_Done(&it));
        break;
    default:
        selva_send_errorf(resp, SELVA_EINVAL, "Unknown array type: %d", subtype);
        break;
    }
}

static void replyWithKeyValue(struct selva_server_response_out *resp, struct selva_string *lang, struct SelvaObjectKey *key) {
    switch (key->type) {
    case SELVA_OBJECT_NULL:
        selva_send_null(resp);
        break;
    case SELVA_OBJECT_DOUBLE:
        selva_send_double(resp, key->emb_double_value);
        break;
    case SELVA_OBJECT_LONGLONG:
        selva_send_ll(resp, key->emb_ll_value);
        break;
    case SELVA_OBJECT_STRING:
        replyWithSelvaString(resp, key->value);
        break;
    case SELVA_OBJECT_OBJECT:
        if (key->value) {
            TO_STR(lang);

            if (key->user_meta == SELVA_OBJECT_META_SUBTYPE_TEXT && lang && lang_len > 0) {
                char buf[lang_len + 1];
                memcpy(buf, lang_str, lang_len + 1);
                const char *sep = "\n";
                char *rest;

                for (const char *s = strtok_r(buf, sep, &rest);
                     s != NULL;
                     s = strtok_r(NULL, sep, &rest)) {
                    const size_t slen = strlen(s);
                    struct SelvaObjectKey *text_key;
                    int err = get_key(key->value, s, slen, 0, &text_key);

                    /* Ignore errors on purpose. */
                    if (!err && text_key->type == SELVA_OBJECT_STRING) {
                        selva_send_string(resp, text_key->value);
                        return;
                    }
                }

                selva_send_null(resp);
            } else {
                replyWithObject(resp, lang, key->value, NULL);
            }
        } else {
            selva_send_null(resp);
        }
        break;
    case SELVA_OBJECT_SET:
        replyWithSelvaSet(resp, &key->selva_set);
        break;
    case SELVA_OBJECT_ARRAY:
        replyWithArray(resp, lang, key->subtype, key->array);
        break;
    case SELVA_OBJECT_POINTER:
        if (key->value) {
            if (key->ptr_opts && key->ptr_opts->ptr_reply) {
                key->ptr_opts->ptr_reply(resp, key->value);
            } else {
                selva_send_str(resp, "(pointer)", 9);
            }
        } else {
            selva_send_str(resp, "(null pointer)", 14);
        }
        break;
    case SELVA_OBJECT_HLL:
        replyWithHll(resp, key->value);
        break;
    default:
        selva_send_errorf(resp, SELVA_EINTYPE, "Type not supported %d", (int)key->type);
    }
}

static void replyWithObject(struct selva_server_response_out *resp, struct selva_string *lang, struct SelvaObject *obj, const char *excluded) {
    struct SelvaObjectKey *key;

    if (!obj) {
        selva_send_null(resp);
        return;
    }

    selva_send_array(resp, -1);

    RB_FOREACH(key, SelvaObjectKeys, &obj->keys_head) {
        const char *name_str = key->name;
        const size_t name_len = key->name_len;

        if (excluded && stringlist_searchn(excluded, name_str, name_len)) {
            continue;
        }

        selva_send_str(resp, key->name, key->name_len);
        replyWithKeyValue(resp, lang, key);
    }

    selva_send_array_end(resp);
}


int SelvaObject_ReplyWithObjectStr(
        struct selva_server_response_out *resp,
        struct selva_string *lang,
        struct SelvaObject *obj,
        const char *key_name_str,
        size_t key_name_len,
        enum SelvaObjectReplyFlags flags __unused) {
    struct SelvaObjectKey *key;
    ssize_t ary_idx, ary_err;
    int err;

    if (!key_name_str) {
        replyWithObject(resp, lang, obj, NULL);
        return 0;
    }

    ary_err = get_array_field_index(key_name_str, key_name_len, &ary_idx);
    if (ary_err < 0) {
        return SELVA_EINVAL;
    } else if (ary_err > 0) {
        size_t new_len = ary_err;

        err = get_key(obj, key_name_str, new_len, 0, &key);
        if (err) {
            return err;
        }

        if (key->type == SELVA_OBJECT_ARRAY) {
            struct SelvaObjectKey k;

            replyWithKeyValue(resp, lang, array_ind_to_key(&k, key, ary_idx));
        } else {
            return SELVA_ENOTSUP; /* How? */
        }
    } else {
        err = get_key(obj, key_name_str, key_name_len, 0, &key);
        if (err) {
            return err;
        }

        replyWithKeyValue(resp, lang, key);
    }

    return 0;
}

int SelvaObject_ReplyWithObject(
        struct selva_server_response_out *resp,
        struct selva_string *lang,
        struct SelvaObject *obj,
        const struct selva_string *key_name,
        enum SelvaObjectReplyFlags flags __unused) {
    if (key_name) {
        TO_STR(key_name);
        return SelvaObject_ReplyWithObjectStr(resp, lang, obj, key_name_str, key_name_len, 0);
    } else {
        replyWithObject(resp, lang, obj, NULL);
        return 0;
    }
}

int SelvaObject_ReplyWithWildcardStr(
        struct selva_server_response_out *resp,
        struct selva_string *lang,
        struct SelvaObject *obj,
        const char *okey_str,
        size_t okey_len,
        long *resp_count,
        int resp_path_start_idx,
        enum SelvaObjectReplyFlags flags) {
    const int idx = (int)((const char *)memmem(okey_str, okey_len, ".*.", 3) - okey_str + 1); /* .*. => *. */

    if (idx > SELVA_OBJECT_KEY_MAX) {
        /*
         * Assume memmem() returned the NULL pointer.
         * The error here matches what get_key() would send,
         * thus not introducing a new error code.
         */
        return SELVA_ENAMETOOLONG;
    }

    /* Path before the wildcard character. */
    const size_t before_len = idx - 1;
    const char *before = okey_str;

    /* Path after the wildcard character. */
    const size_t after_len = okey_len - idx - 2;
    const char *after = &okey_str[idx + 2];

    struct SelvaObjectKey *key;
    int err;

    err = get_key(obj, before, before_len, 0, &key);
    if (!err && (key->type != SELVA_OBJECT_OBJECT ||
        ((flags & SELVA_OBJECT_REPLY_ANY_OBJ_FLAG) == 0 && key->user_meta != SELVA_OBJECT_META_SUBTYPE_RECORD))) {
        err = SELVA_ENOENT;
    } else if (!err) {
        void *it = SelvaObject_ForeachBegin(key->value);
        const char *obj_key_name_str;

        while ((obj_key_name_str = SelvaObject_ForeachKey(key->value, &it))) {
            /*
             *  Construct a new field path with the resolved path with the following:
             *  -> path before the wildcard
             *  -> the current object key being iterated
             *  -> path after the wildcard
             */
            const size_t obj_key_len = strlen(obj_key_name_str);
            const size_t new_field_len = before_len + 1 + obj_key_len + 1 + after_len;
            char new_field[new_field_len + 1];

            snprintf(new_field, new_field_len + 1, "%.*s.%.*s.%.*s",
                    (int)before_len, before,
                    (int)obj_key_len, obj_key_name_str,
                    (int)after_len, after);

            if (memmem(new_field, new_field_len, ".*.", 3)) {
                /* Recurse for nested wildcards while keeping the resolved path. */
                SelvaObject_ReplyWithWildcardStr(resp, lang, obj, new_field, new_field_len,
                                                 resp_count,
                                                 resp_path_start_idx == -1 ? idx : resp_path_start_idx,
                                                 flags);
                continue;
            }

            struct SelvaObjectKey *key; /* Note that we shadow the variable here. */
            err = get_key(obj, new_field, new_field_len, 0, &key);
            if (err) {
                /*
                 * This is an unlikely event because the field is known to exist,
                 * but if it happens we must skip to avoid segfaulting.
                 */
                SELVA_LOG(SELVA_LOGL_DBG, "Failed to get value for \"%.*s\"",
                          (int)new_field_len, new_field);
                continue;
            }

            if (flags & SELVA_OBJECT_REPLY_SPLICE_FLAG) {
                /* if the path should be spliced to start from the first wildcard as expected by selva.object.get */
                const size_t reply_path_len = resp_path_start_idx == -1
                    ? obj_key_len + 1 + after_len
                    : (before_len - resp_path_start_idx) + 1 + obj_key_len + 1 + after_len;
                char reply_path[reply_path_len + 1];

                if (resp_path_start_idx == -1) {
                    snprintf(reply_path, reply_path_len + 1, "%.*s.%.*s",
                        (int)obj_key_len, obj_key_name_str,
                        (int)after_len, after);
                } else {
                    snprintf(reply_path, reply_path_len + 1, "%.*s.%.*s.%.*s",
                        (int)(before_len - resp_path_start_idx), before + resp_path_start_idx,
                        (int)obj_key_len, obj_key_name_str,
                        (int)after_len, after);
                }

                selva_send_str(resp, reply_path, reply_path_len);
            } else {
                /* if the whole resolved path should be returned */
                const size_t reply_path_len = before_len + 1 + obj_key_len + 1 + key->name_len;
                char reply_path[reply_path_len + 1];

                snprintf(reply_path, reply_path_len + 1, "%.*s.%.*s.%.*s",
                        (int)before_len, before,
                        (int)obj_key_len, obj_key_name_str,
                        (int)key->name_len, key->name);
                selva_send_str(resp, reply_path, reply_path_len);
            }

            replyWithKeyValue(resp, lang, key);
            *resp_count += 2;
        }
    }

    /* ignore errors unless nothing was returned and an error occurred */
    if (err && *resp_count == 0) {
        return err;
    }

    return 0;
}

static int load_object_double(struct selva_io *io, struct SelvaObject *obj, const struct selva_string *name) {
    double value;
    int err;

    value = selva_io_load_double(io);
    err = SelvaObject_SetDouble(obj, name, value);
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Error while loading a double");
        return SELVA_EINVAL;
    }

    return 0;
}

static int load_object_long_long(struct selva_io *io, struct SelvaObject *obj, const struct selva_string *name) {
    long long value;
    int err;

    value = selva_io_load_signed(io);
    err = SelvaObject_SetLongLong(obj, name, value);
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Error while loading a long long");
        return SELVA_EINVAL;
    }

    return 0;
}

static int load_object_string(struct selva_io *io, int level, struct SelvaObject *obj, const struct selva_string *name) {
    TO_STR(name);
    struct selva_string *value = selva_io_load_string(io);

    if (level == 0 && SELVA_IS_TYPE_FIELD(name_str, name_len)) {
        TO_STR(value);
        struct selva_string *shared;
        int err;

        shared = selva_string_create(value_str, value_len, SELVA_STRING_INTERN);

        selva_string_free(value);

        err = SelvaObject_SetStringStr(obj, name_str, name_len, shared);
        if (err) {
            SELVA_LOG(SELVA_LOGL_CRIT,  "Failed to set a shared string value");
            return err;
        }
    } else {
        int err;

        err = SelvaObject_SetStringStr(obj, name_str, name_len, value);
        if (err) {
            SELVA_LOG(SELVA_LOGL_CRIT, "Error while loading a string");
            return SELVA_EINVAL;
        }
    }

    return 0;
}

static int load_object_set(struct selva_io *io, struct SelvaObject *obj, const struct selva_string *name) {
    enum SelvaSetType setType = selva_io_load_unsigned(io);
    const size_t n = selva_io_load_unsigned(io);

    if (setType == SELVA_SET_TYPE_STRING) {
        for (size_t j = 0; j < n; j++) {
            struct selva_string *value = selva_io_load_string(io);

            SelvaObject_AddStringSet(obj, name, value);
        }
    } else if (setType == SELVA_SET_TYPE_DOUBLE) {
        for (size_t j = 0; j < n; j++) {
            double value = selva_io_load_double(io);

            SelvaObject_AddDoubleSet(obj, name, value);
        }
    } else if (setType == SELVA_SET_TYPE_LONGLONG) {
        for (size_t j = 0; j < n; j++) {
            long long value = selva_io_load_signed(io);

            SelvaObject_AddLongLongSet(obj, name, value);
        }
    } else {
        SELVA_LOG(SELVA_LOGL_CRIT, "Unknown set type");
        return SELVA_EINTYPE;
    }

    return 0;
}

static int load_object_array(struct selva_io *io, struct SelvaObject *obj, const struct selva_string *name, int encver, void *ptr_load_data) {
    TO_STR(name);
    enum SelvaObjectType arrayType = selva_io_load_unsigned(io);
    const size_t n = selva_io_load_unsigned(io);
    struct SelvaObjectKey *key;
    int err;

    err = get_key_array_modify(obj, name_str, name_len, arrayType, n, &key);
    if (err) {
        return err;
    }

    switch (arrayType) {
    case SELVA_OBJECT_LONGLONG:
        for (size_t i = 0; i < n; i++) {
            long long value = selva_io_load_signed(io);
            SVector_Insert(key->array, (void *)value);
        }
        break;
    case SELVA_OBJECT_DOUBLE:
        for (size_t i = 0; i < n; i++) {
            double value = selva_io_load_double(io);
            void *wrapper;
            memcpy(&wrapper, &value, sizeof(value));
            SVector_Insert(key->array, wrapper);
        }
        break;
    case SELVA_OBJECT_STRING:
        for (size_t i = 0; i < n; i++) {
            struct selva_string *value = selva_io_load_string(io);
            size_t value_len;

            (void)selva_string_to_str(value, &value_len);
            if (value_len) {
                SVector_SetIndex(key->array, i, value);
            }
        }
        break;
    case SELVA_OBJECT_OBJECT:
        for (size_t i = 0; i < n; i++) {
            struct SelvaObject *o = SelvaObjectTypeLoad(io, encver, ptr_load_data);
            /* FIXME fix empty indices, this load doesn't load correctly */
            SVector_Insert(key->array, o);
        }
        break;
    case SELVA_OBJECT_HLL:
        for (size_t i = 0; i < n; i++) {
            struct selva_string *value = selva_io_load_string(io);
            TO_STR(value);

            if (value_len) {
                hll_t *hll = hll_restore(value_str, value_len);
                selva_string_free(value);
                SVector_SetIndex(key->array, i, hll);
            }
        }
        break;
    default:
        SELVA_LOG(SELVA_LOGL_CRIT, "Unknown array type");
        return SELVA_EINTYPE;
    }

    return 0;
}

static int load_pointer(struct selva_io *io, int encver, struct SelvaObject *obj, const struct selva_string *name, void *ptr_load_data) {
    unsigned ptr_type_id;

    ptr_type_id = selva_io_load_unsigned(io);
    if (ptr_type_id > 0) {
        const struct SelvaObjectPointerOpts *opts;
        int err;
        void *p;

        opts = get_ptr_opts(ptr_type_id);
        if (!(opts && opts->ptr_load)) {
            SELVA_LOG(SELVA_LOGL_CRIT, "No ptr_load given");
            return SELVA_EINVAL; /* Presumably a serialized pointer should have a loader fn. */
        }

        p = opts->ptr_load(io, encver, ptr_load_data);
        if (!p) {
            SELVA_LOG(SELVA_LOGL_CRIT, "Failed to load a SELVA_OBJECT_POINTER");
            return SELVA_EGENERAL;
        }

        err = SelvaObject_SetPointer(obj, name, p, opts);
        if (err) {
            SELVA_LOG(SELVA_LOGL_CRIT, "Failed to load a SELVA_OBJECT_POINTER: %s",
                      selva_strerror(err));
            return SELVA_EGENERAL;
        }
    } else {
        SELVA_LOG(SELVA_LOGL_CRIT, "ptr_type_id shouldn't be 0");
    }

    return 0;
}

static int load_hll(struct selva_io *io, struct SelvaObject *obj, const struct selva_string *name)
{
    struct selva_string *value = selva_io_load_string(io);
    TO_STR(name, value);
    struct SelvaObjectKey *key;
    int err;

    err = get_key(obj, name_str, name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
    if (err) {
        return err;
    }

    clear_key_value(key);
    key->type = SELVA_OBJECT_HLL;
    key->value = hll_restore(value_str, value_len);
    selva_string_free(value);

    if (!key->value) {
        /* FIXME Must not set value if hll is not valid */
        return SELVA_EINVAL;
    }

    return 0;
}

static int load_field(struct selva_io *io, struct SelvaObject *obj, int encver, int level, void *ptr_load_data) {
    struct selva_string *name = selva_io_load_string(io);
    const enum SelvaObjectType type = selva_io_load_unsigned(io);
    const SelvaObjectMeta_t user_meta = selva_io_load_unsigned(io);
    int err = 0;

    if (unlikely(!name)) {
        SELVA_LOG(SELVA_LOGL_CRIT, "SelvaObject key name cannot be NULL");
        return SELVA_EINVAL;
    }

    switch (type) {
    case SELVA_OBJECT_NULL:
        /* NOP - There is generally no reason to recreate NULLs */
        break;
    case SELVA_OBJECT_DOUBLE:
        err = load_object_double(io, obj, name);
        break;
    case SELVA_OBJECT_LONGLONG:
        err = load_object_long_long(io, obj, name);
        break;
    case SELVA_OBJECT_STRING:
        err = load_object_string(io, level, obj, name);
        break;
    case SELVA_OBJECT_OBJECT:
        {
            struct SelvaObjectKey *key;
            TO_STR(name);

            err = get_key(obj, name_str, name_len, SELVA_OBJECT_GETKEY_CREATE, &key);
            if (err) {
                break;
            }

            key->value = load_object(io, encver, level + 1, ptr_load_data);
            if (!key->value) {
                err = SELVA_EINVAL;
                break;
            }
            key->type = SELVA_OBJECT_OBJECT;
        }
        break;
    case SELVA_OBJECT_SET:
        err = load_object_set(io, obj, name);
        break;
    case SELVA_OBJECT_ARRAY:
        err = load_object_array(io, obj, name, encver, ptr_load_data);
        break;
    case SELVA_OBJECT_POINTER:
        err = load_pointer(io, encver, obj, name, ptr_load_data);
        break;
    case SELVA_OBJECT_HLL:
        err = load_hll(io, obj, name);
        break;
    default:
        SELVA_LOG(SELVA_LOGL_CRIT, "Unknown type");
    }
    if (err) {
            SELVA_LOG(SELVA_LOGL_CRIT, "Error while loading a %s: %s",
                      SelvaObject_Type2String(type, NULL),
                      selva_strerror(err));
        return err;
    }

    /*
     * Not the most efficient way to do this as we may need to look
     * multiple lookups.
     */
    err = SelvaObject_SetUserMeta(obj, name, user_meta, NULL);
    if (err) {
        TO_STR(name);

        /*
         * This could be critical but sometimes we might just decide to not
         * create something like an empty array at this point.
         */
        SELVA_LOG(SELVA_LOGL_WARN, "Failed to set user meta on \"%.*s\": %s",
                  (int)name_len, name_str, selva_strerror(err));
    }

    selva_string_free(name);
    return 0;
}

static struct SelvaObject *load_object_to(struct selva_io *io, int encver, struct SelvaObject *obj, int level, void *ptr_load_data) {
    const size_t obj_size = selva_io_load_unsigned(io);
    for (size_t i = 0; i < obj_size; i++) {
        int err;

        err = load_field(io, obj, encver, level, ptr_load_data);
        if (err) {
            /* No need to do cleanup as we will terminate. */
            return NULL;
        }
    }

    return obj;
}

static struct SelvaObject *load_object(struct selva_io *io, int encver, int level, void *ptr_load_data) {
    return load_object_to(io, encver, SelvaObject_New(), level, ptr_load_data);
}

struct SelvaObject *SelvaObjectTypeLoadTo(struct selva_io *io, int encver, struct SelvaObject *obj, void *ptr_load_data) {
    return load_object_to(io, encver, obj, 0, ptr_load_data);
}

struct SelvaObject *SelvaObjectTypeLoad(struct selva_io *io, int encver, void *ptr_load_data) {
    struct SelvaObject *obj;

    obj = load_object(io, encver, 0, ptr_load_data);

    return obj;
}

struct SelvaObject *SelvaObjectTypeLoad2(struct selva_io *io, int encver, void *ptr_load_data) {
    const size_t obj_size = selva_io_load_unsigned(io);
    struct SelvaObject *obj;

    if (obj_size == 0) {
        return NULL;
    }

    obj = SelvaObject_New();

    for (size_t i = 0; i < obj_size; i++) {
        int err;

        err = load_field(io, obj, encver, 0, ptr_load_data);
        if (err) {
            /* No need to do cleanup as we will terminate. */
            return NULL;
        }
    }

    return obj;
}

static void save_object_string(struct selva_io *io, struct SelvaObjectKey *key) {
    if (!key->value) {
        SELVA_LOG(SELVA_LOGL_CRIT, "STRING value missing");
        selva_io_save_str(io, "", 0);
    }
    selva_io_save_string(io, key->value);
}

static void save_object_set(struct selva_io *io, struct SelvaObjectKey *key) {
    const struct SelvaSet *selva_set = &key->selva_set;

    selva_io_save_unsigned(io, selva_set->type);
    selva_io_save_unsigned(io, selva_set->size);

    if (selva_set->type == SELVA_SET_TYPE_STRING) {
        struct SelvaSetElement *el;

        SELVA_SET_STRING_FOREACH(el, &key->selva_set) {
            selva_io_save_string(io, el->value_string);
        }
    } else if (selva_set->type == SELVA_SET_TYPE_DOUBLE) {
        struct SelvaSetElement *el;

        SELVA_SET_DOUBLE_FOREACH(el, &key->selva_set) {
            selva_io_save_double(io, el->value_d);
        }
    } else if (selva_set->type == SELVA_SET_TYPE_LONGLONG) {
        struct SelvaSetElement *el;

        SELVA_SET_LONGLONG_FOREACH(el, &key->selva_set) {
            selva_io_save_signed(io, el->value_ll);
        }
    } else {
        SELVA_LOG(SELVA_LOGL_CRIT, "Unknown set type");
    }
}

static void save_object_hll(struct selva_io *io, void *value) {
    hll_t *hll = value;

    if (hll) {
        size_t len;
        const char *str = hll_getstr(hll, &len);
        selva_io_save_str(io, str, len);
    } else {
        selva_io_save_str(io, "", 0);
    }
}

static void save_object_array(struct selva_io *io, struct SelvaObjectKey *key, void *ptr_save_data) {
    const struct SVector *array = key->array;
    const size_t array_size = SVector_Size(array);

    selva_io_save_unsigned(io, key->subtype);
    selva_io_save_unsigned(io, array_size);

    if (key->subtype == SELVA_OBJECT_LONGLONG) {
        for (size_t i = 0; i < array_size; i++) {
            void *num;

            num = SVector_GetIndex(array, i);
            selva_io_save_signed(io, (long long)num);
        }
    } else if (key->subtype == SELVA_OBJECT_DOUBLE) {
        for (size_t i = 0; i < array_size; i++) {
            void *num_ptr;
            double num;

            num_ptr = SVector_GetIndex(array, i);
            memcpy(&num, &num_ptr, sizeof(double));
            selva_io_save_double(io, num);
        }
    } else if (key->subtype == SELVA_OBJECT_STRING) {
        for (size_t i = 0; i < array_size; i++) {
            struct selva_string *s;

            s = SVector_GetIndex(array, i);
            TO_STR(s);

            selva_io_save_str(io, s_str, s_len);
        }
    } else if (key->subtype == SELVA_OBJECT_OBJECT) {
        for (size_t i = 0; i < array_size; i++) {
            struct SelvaObject *k;

            k = SVector_GetIndex(array, i);
            SelvaObjectTypeSave2(io, k, ptr_save_data);
        }
    } else if (key->subtype == SELVA_OBJECT_HLL) {
        for (size_t i = 0; i < array_size; i++) {
            save_object_hll(io, SVector_GetIndex(array, i));
        }
    } else {
        SELVA_LOG(SELVA_LOGL_CRIT, "Unknown object array type");
    }
}

void SelvaObjectTypeSave(struct selva_io *io, struct SelvaObject *obj, void *ptr_save_data) {
    struct SelvaObjectKey *key;

    if (unlikely(!obj)) {
        SELVA_LOG(SELVA_LOGL_CRIT, "obj can't be NULL");
        return;
    }

    selva_io_save_unsigned(io, obj->obj_size);
    RB_FOREACH(key, SelvaObjectKeys, &obj->keys_head) {
        selva_io_save_str(io, key->name, key->name_len);
        selva_io_save_unsigned(io, key->type);
        selva_io_save_unsigned(io, key->user_meta);

        switch (key->type) {
        case SELVA_OBJECT_NULL:
            /* null is implicit value and doesn't need to be persisted. */
            break;
        case SELVA_OBJECT_DOUBLE:
            selva_io_save_double(io, key->emb_double_value);
            break;
        case SELVA_OBJECT_LONGLONG:
            selva_io_save_signed(io, key->emb_ll_value);
            break;
        case SELVA_OBJECT_STRING:
            save_object_string(io, key);
            break;
        case SELVA_OBJECT_OBJECT:
            if (!key->value) {
                SELVA_LOG(SELVA_LOGL_CRIT, "OBJECT value missing");
                break;
            }
            SelvaObjectTypeSave(io, key->value, ptr_save_data);
            break;
        case SELVA_OBJECT_SET:
            save_object_set(io, key);
            break;
        case SELVA_OBJECT_ARRAY:
            save_object_array(io, key, ptr_save_data);
            break;
        case SELVA_OBJECT_POINTER:
            if (key->ptr_opts && key->ptr_opts->ptr_save) {
                selva_io_save_unsigned(io, key->ptr_opts->ptr_type_id); /* This is used to locate the loader. */
                key->ptr_opts->ptr_save(io, key->value, ptr_save_data);
            } else {
                SELVA_LOG(SELVA_LOGL_CRIT, "ptr_save() not given");
                break;
            }
            break;
        case SELVA_OBJECT_HLL:
            save_object_hll(io, key->value);
            break;
        default:
            SELVA_LOG(SELVA_LOGL_CRIT, "Unknown type");
        }
    }
}

void SelvaObjectTypeSave2(struct selva_io *io, struct SelvaObject *obj, void *ptr_save_data) {
    if (!obj) {
        selva_io_save_unsigned(io, 0);
    } else {
        SelvaObjectTypeSave(io, obj, ptr_save_data);
    }
}
