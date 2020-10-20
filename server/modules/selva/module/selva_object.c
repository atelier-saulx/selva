#include <assert.h>
#include <limits.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "errors.h"
#include "cstrings.h"
#include "tree.h"
#include "selva_onload.h"
#include "cdefs.h"

#define SELVA_OBJECT_ENCODING_VERSION   0
#define SELVA_OBJECT_KEY_MAX            USHRT_MAX

#define SELVA_OBJECT_GETKEY_CREATE      0x1 /*!< Create the key and required nested objects. */
#define SELVA_OBJECT_GETKEY_DELETE      0x2 /*!< Delete the key found. */

enum SelvaObjectType {
    SELVA_OBJECT_NULL,
    SELVA_OBJECT_DOUBLE,
    SELVA_OBJECT_LONGLONG,
    SELVA_OBJECT_OBJECT,
    SELVA_OBJECT_STRING,
};

RB_HEAD(SelvaObjectKeys, SelvaObjectKey);

struct SelvaObjectKey {
    enum SelvaObjectType type;
    unsigned short name_len;
    RB_ENTRY(SelvaObjectKey) _entry;
    union {
        void *value;
        double emb_double_value;
        long long emb_ll_value;
    };
    char name[0];
};

struct SelvaObject {
    struct SelvaObjectKeys keys_head;
};

static RedisModuleType *ObjectType;

static void destroy_selva_object(struct SelvaObject *obj);
static int get_key(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len, unsigned flags, struct SelvaObjectKey **out);
RB_PROTOTYPE_STATIC(SelvaObjectKeys, SelvaObjectKey, _entry, SelvaObject_Compare)

static int SelvaObject_Compare(struct SelvaObjectKey *a, struct SelvaObjectKey *b) {
    return strcmp(a->name, b->name);
}

RB_GENERATE_STATIC(SelvaObjectKeys, SelvaObjectKey, _entry, SelvaObject_Compare)

static struct SelvaObject *new_selva_object(void) {
    struct SelvaObject *obj;

    obj = RedisModule_Alloc(sizeof(*obj));
    RB_INIT(&obj->keys_head);

    return obj;
}

static int clear_key_value(struct SelvaObjectKey *key) {
    switch (key->type) {
    /* TODO Other types */
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

            destroy_selva_object(obj);
        }
        break;
    default:
        fprintf(stderr, "%s: Unknown object value type (%d)\n", __FILE__, (int)key->type);
        return SELVA_EINTYPE;
    }

    key->type = SELVA_OBJECT_NULL;

    return 0;
}

static void destroy_selva_object(struct SelvaObject *obj) {
    struct SelvaObjectKey *key;
    struct SelvaObjectKey *next;

	for (key = RB_MIN(SelvaObjectKeys, &obj->keys_head); key != NULL; key = next) {
		next = RB_NEXT(SelvaObjectKeys, &obj->keys_head, key);
		RB_REMOVE(SelvaObjectKeys, &obj->keys_head, key);
        (void)clear_key_value(key);
        RedisModule_Free(key);
    }

    RedisModule_Free(obj);
}

size_t SelvaObject_MemUsage(const void *value) {
    struct SelvaObject *obj = (struct SelvaObject *)value;
    struct SelvaObjectKey *key;
    size_t size = sizeof(*obj);

    RB_FOREACH(key, SelvaObjectKeys, &obj->keys_head) {
        size += sizeof(*key) + key->name_len + 1;

        switch (key->type) {
        case SELVA_OBJECT_STRING:
            {
                size_t len;

                if (key->value) {
                    (void)RedisModule_StringPtrLen(key->value, &len);
                    size += len + 1;
                }
            }
            break;
        case SELVA_OBJECT_OBJECT:
            // FIXME
            fprintf(stderr, "%s: obj size calc not implemented\n", __FILE__);
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
            obj = new_selva_object();
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

    /* Create an empty value object if the key is currently empty. */
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        int err;

        obj = new_selva_object();
        if (!obj) {
            return SELVA_ENOMEM;
        }

        err = RedisModule_ModuleTypeSetValue(key, ObjectType, obj);
        if (err != REDISMODULE_OK) {
            destroy_selva_object(obj);
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
    const size_t nr_parts = substring_count(key_name_str, ".") + 1;
    char buf[key_name_len]; /* We assume that the length has been sanity checked at this point. */
    char *s = buf;
    struct SelvaObjectKey *key;
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
                const size_t key_size = sizeof(struct SelvaObjectKey) + slen + 1;
                key = RedisModule_Alloc(key_size);
                if (!key) {
                    return SELVA_ENOMEM;
                }

                memset(key, 0, key_size);
                strcpy(key->name, s); /* strok() is safe. */
                key->name_len = slen;
            } else {
                /*
                 * Clear the old value.
                 */
                clear_key_value(key);
            }
            key->type = SELVA_OBJECT_OBJECT;
            key->value = new_selva_object();
            (void)RB_INSERT(SelvaObjectKeys, &obj->keys_head, key);

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
    if (nr_parts_found != nr_parts) {
        return SELVA_ENOENT;
    }

    if (flags & SELVA_OBJECT_GETKEY_DELETE) {
        RB_REMOVE(SelvaObjectKeys, &cobj->keys_head, key);
        (void)clear_key_value(key);
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

    if (strstr(key_name_str, ".")) {
        return get_key_obj(obj, key_name_str, key_name_len, flags, out);
    }

    const size_t key_size = sizeof(struct SelvaObjectKey) + key_name_len + 1;
    char buf[key_size] __attribute__((aligned(alignof(struct SelvaObjectKey)))); /* RFE This might be dumb */

    filter = (struct SelvaObjectKey *)buf;
    memset(filter, 0, key_size);
    memcpy(filter->name, key_name_str, key_name_len + 1);
    filter->name_len = key_name_len;

    key = RB_FIND(SelvaObjectKeys, &obj->keys_head, filter);
    if (!key && (flags & SELVA_OBJECT_GETKEY_CREATE)) {
        key = RedisModule_Alloc(key_size);
        if (!key) {
            return SELVA_ENOMEM;
        }

        memcpy(key, filter, key_size);
        memset(&key->_entry, 0, sizeof(key->_entry)); /* RFE Might not be necessary. */
        (void)RB_INSERT(SelvaObjectKeys, &obj->keys_head, key);
    } else if (!key) {
        return SELVA_ENOENT;
    }

    if (flags & SELVA_OBJECT_GETKEY_DELETE) {
        RB_REMOVE(SelvaObjectKeys, &obj->keys_head, key);
        (void)clear_key_value(key);
        RedisModule_Free(key);
        key = NULL;
    }

    *out = key;
    return 0;
}

int SelvaObject_DelKey(struct SelvaObject *obj, const RedisModuleString *key_name) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, SELVA_OBJECT_GETKEY_DELETE, &key);
    if (err) {
        return err;
    }

    return 0;
}

int SelvaObject_Exists(struct SelvaObject *obj, const RedisModuleString *key_name) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);
    int err;

    assert(obj);

    err = get_key(obj, key_name_str, key_name_len, 0, &key);
    if (err) {
        return err;
    }

    return 0;
}

int SelvaObject_GetDouble(struct SelvaObject *obj, const RedisModuleString *key_name, double *out) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);
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

int SelvaObject_GetLongLong(struct SelvaObject *obj, const RedisModuleString *key_name, long long *out) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);
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

int SelvaObject_GetStr(struct SelvaObject *obj, const RedisModuleString *key_name, RedisModuleString **out) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);
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

int SelvaObject_SetDouble(struct SelvaObject *obj, const RedisModuleString *key_name, double value) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);
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

int SelvaObject_SetLongLong(struct SelvaObject *obj, const RedisModuleString *key_name, double value) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);
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

int SelvaObject_SetStr(struct SelvaObject *obj, const RedisModuleString *key_name, RedisModuleString *value) {
    struct SelvaObjectKey *key;
    TO_STR(key_name);
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

    RedisModule_RetainString(NULL, value);
    key->type = SELVA_OBJECT_STRING;
    key->value = value;

    return 0;
}

int SelvaObject_DelCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    int err;

    const size_t ARGV_KEY = 1;
    const size_t ARGV_OKEY = 2;

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return replyWithSelvaError(ctx, SELVA_ENOENT);
    }

    err = SelvaObject_DelKey(obj, argv[ARGV_OKEY]);
    if (err == SELVA_ENOENT) {
        RedisModule_ReplyWithLongLong(ctx, 0);
    } else if (err) {
        return replyWithSelvaError(ctx, err);
    }
    RedisModule_ReplyWithLongLong(ctx, 1);

    return RedisModule_ReplicateVerbatim(ctx);
}

int SelvaObject_ExistsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    int err;

    const size_t ARGV_KEY = 1;
    const size_t ARGV_OKEY = 2;

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return replyWithSelvaError(ctx, SELVA_ENOENT);
    }

    err = SelvaObject_Exists(obj, argv[ARGV_OKEY]);
    if (err == SELVA_ENOENT) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    } else if (err) {
        return replyWithSelvaError(ctx, err);
    }
    RedisModule_ReplyWithLongLong(ctx, 1);

    return RedisModule_ReplicateVerbatim(ctx);
}

int SelvaObject_GetCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    struct SelvaObjectKey *key;
    int err;

    const size_t ARGV_KEY = 1;
    const size_t ARGV_OKEY = 2;

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return replyWithSelvaError(ctx, SELVA_ENOENT);
    }

    RedisModuleString *okey = argv[ARGV_OKEY];
    TO_STR(okey);
    err = get_key(obj, okey_str, okey_len, 0, &key);
    if (err == SELVA_ENOENT) {
        return RedisModule_ReplyWithNull(ctx);
    } else if (err) {
        return replyWithSelvaErrorf(ctx, err, "get_key");
    }

    switch (key->type) {
    case SELVA_OBJECT_NULL:
        return RedisModule_ReplyWithNull(ctx);
        break;
    case SELVA_OBJECT_DOUBLE:
        RedisModule_ReplyWithDouble(ctx, key->emb_double_value);
        break;
    case SELVA_OBJECT_LONGLONG:
        RedisModule_ReplyWithLongLong(ctx, key->emb_ll_value);
        break;
    case SELVA_OBJECT_STRING:
        RedisModule_ReplyWithString(ctx, key->value);
        break;
    default:
        RedisModule_ReplyWithError(ctx, "type error");
    }

    return REDISMODULE_OK;
}

static void send_object(RedisModuleCtx *ctx, struct SelvaObject *obj) {
    struct SelvaObjectKey *key;
    size_t n = 0;

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    RB_FOREACH(key, SelvaObjectKeys, &obj->keys_head) {
        RedisModule_ReplyWithStringBuffer(ctx, key->name, key->name_len);

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
                break;
            }
        case SELVA_OBJECT_OBJECT:
            if (key->value) {
                send_object(ctx, key->value);
                break;
            }
        default:
            RedisModule_ReplyWithError(ctx, "type error");
        }

        n += 2;
    }
    RedisModule_ReplySetArrayLength(ctx, n);
}

int SelvaObject_GetAllCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;

    const size_t ARGV_KEY = 1;

    if (argc < 2) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!obj) {
        return replyWithSelvaError(ctx, SELVA_ENOENT);
    }

    send_object(ctx, obj);

    return REDISMODULE_OK;
}

int SelvaObject_SetCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    const char *type;
    struct SelvaObject *obj;
    int err;

    const size_t ARGV_KEY = 1;
    const size_t ARGV_OKEY = 2;
    const size_t ARGV_TYPE = 3;
    const size_t ARGV_OVAL = 4;

    if (argc < 5) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!obj) {
        return replyWithSelvaError(ctx, SELVA_ENOENT);
    }

    /* TODO parse type & set by type */
    type = RedisModule_StringPtrLen(argv[ARGV_TYPE], NULL);

    switch (type[0]) {
    case 'f': /* SELVA_OBJECT_DOUBLE */
        err = SelvaObject_SetDouble(
            obj,
            argv[ARGV_OKEY],
            strtod(RedisModule_StringPtrLen(argv[ARGV_OVAL], NULL), NULL));
        break;
    case 'i': /* SELVA_OBJECT_LONGLONG */
        err = SelvaObject_SetLongLong(
            obj,
            argv[ARGV_OKEY],
            strtoll(RedisModule_StringPtrLen(argv[ARGV_OVAL], NULL), NULL, 10));
        break;
    case 's': /* SELVA_OBJECT_STRING */
        err = SelvaObject_SetStr(obj, argv[ARGV_OKEY], argv[ARGV_OVAL]);
        break;
    default:
        err = SELVA_EINTYPE;
    }
    if (err) {
        return replyWithSelvaError(ctx, err);
    }
    RedisModule_ReplyWithLongLong(ctx, 1);

    return RedisModule_ReplicateVerbatim(ctx);
}

int SelvaObject_TypeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;
    struct SelvaObjectKey *key;
    int err;

    const size_t ARGV_KEY = 1;
    const size_t ARGV_OKEY = 2;

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return replyWithSelvaError(ctx, SELVA_ENOENT);
    }

    const char type_null[] = "null";
    const char type_double[] = "double";
    const char type_longlong[] = "long long";
    const char type_string[] = "string";
    const char type_object[] = "object";
#define REPLY_WITH_TYPE(str) \
    RedisModule_ReplyWithStringBuffer(ctx, (str), sizeof(str) - 1)

    RedisModuleString *okey = argv[ARGV_OKEY];
    TO_STR(okey);
    err = get_key(obj, okey_str, okey_len, 0, &key);
    if (err == SELVA_ENOENT) {
        return REPLY_WITH_TYPE(type_null);
    } else if (err) {
        return replyWithSelvaErrorf(ctx, err, "get_key");
    }

    switch (key->type) {
    case SELVA_OBJECT_NULL:
        REPLY_WITH_TYPE(type_null);
        break;
    case SELVA_OBJECT_DOUBLE:
        REPLY_WITH_TYPE(type_double);
        break;
    case SELVA_OBJECT_LONGLONG:
        REPLY_WITH_TYPE(type_longlong);
        break;
    case SELVA_OBJECT_STRING:
        REPLY_WITH_TYPE(type_string);
        break;
    case SELVA_OBJECT_OBJECT:
        REPLY_WITH_TYPE(type_object);
        break;
    default:
        fprintf(stderr, "%s: Invalid type %d\n", __FILE__, key->type);
        RedisModule_ReplyWithError(ctx, "type error");
    }

#undef REPLY_WITH_TYPE
    return REDISMODULE_OK;
}

void *SelvaObjectTypeRDBLoad(RedisModuleIO *io, int encver) {
    struct SelvaObject *obj;

    if (encver != SELVA_OBJECT_ENCODING_VERSION) {
        /*
         * RFE
         * We should actually log an error here, or try to implement
         * the ability to load older versions of our data structure.
         */
        return NULL;
    }

    obj = new_selva_object();
    if (!obj) {
        RedisModule_LogIOError(io, "warning", "Failed to create a new SelvaObject");
        return NULL;
    }

    int err;
    int done = 0;
    while (!done) {
        RedisModuleString *name;
        enum SelvaObjectType type;

        name = RedisModule_LoadString(io);
        type = RedisModule_LoadUnsigned(io);

        switch (type) {
        case SELVA_OBJECT_NULL:
            done = 1;
            break;
        case SELVA_OBJECT_DOUBLE:
            {
                double value;

                value = RedisModule_LoadDouble(io);
                err = SelvaObject_SetDouble(obj, name, value);
                if (err) {
                    RedisModule_LogIOError(io, "warning", "Error while loading a double");
                    return NULL;
                }
            }
            break;
        case SELVA_OBJECT_LONGLONG:
            {
                long long value;

                value = RedisModule_LoadSigned(io);
                err = SelvaObject_SetLongLong(obj, name, value);
                if (err) {
                    RedisModule_LogIOError(io, "warning", "Error while loading a long long");
                    return NULL;
                }
            }
            break;
        case SELVA_OBJECT_STRING:
            {
                RedisModuleString *value;

                value = RedisModule_LoadString(io);
                err = SelvaObject_SetStr(obj, name, value);
                if (err) {
                    RedisModule_LogIOError(io, "warning", "Error while loading a string");
                    return NULL;
                }

                RedisModule_FreeString(NULL, value);
            }
            break;
        default:
            RedisModule_LogIOError(io, "warning", "Unknown type");
        }

        RedisModule_FreeString(NULL, name);
    }

    return obj;
}

void SelvaObjectTypeRDBSave(RedisModuleIO *io, void *value) {
    struct SelvaObject *obj = (struct SelvaObject *)value;
    struct SelvaObjectKey *key;

    RB_FOREACH(key, SelvaObjectKeys, &obj->keys_head) {
        RedisModule_SaveStringBuffer(io, key->name, key->name_len);

        /* TODO Other types */
        if (key->type != SELVA_OBJECT_NULL) {
            if (!key->value) {
                RedisModule_LogIOError(io, "warning", "Value is NULL");
                continue;
            }

            RedisModule_SaveUnsigned(io, key->type);

            switch (key->type) {
            case SELVA_OBJECT_DOUBLE:
                RedisModule_SaveDouble(io, key->emb_double_value);
                break;
            case SELVA_OBJECT_LONGLONG:
                RedisModule_SaveSigned(io, key->emb_ll_value);
                break;
            case SELVA_OBJECT_STRING:
                RedisModule_SaveString(io, key->value);
                break;
            default:
                RedisModule_LogIOError(io, "warning", "Unknown type");
            }
        }
    }

    const char term[] = "___selva$terminator";
    RedisModule_SaveStringBuffer(io, term, sizeof(term) - 1);   /* key name */
    RedisModule_SaveUnsigned(io, SELVA_OBJECT_NULL);            /* key type */
    /* nil                                                         value    */
}

void SelvaObjectTypeAOFRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    struct SelvaObject *obj = (struct SelvaObject *)value;
    struct SelvaObjectKey *okey;

    RB_FOREACH(okey, SelvaObjectKeys, &obj->keys_head) {
        switch (okey->type) {
        case SELVA_OBJECT_NULL:
            /* NOP - NULL is implicit */
            break;
        case SELVA_OBJECT_DOUBLE:
            {
                RedisModuleString *v;
                v = RedisModule_CreateStringPrintf(NULL, "%f", okey->emb_double_value);
                RedisModule_EmitAOF(aof, "SELVA.OBJECT.SET", "sbs",
                    key,
                    okey->name, (size_t)okey->name_len,
                    v);
                RedisModule_FreeString(NULL, v);
            }
            break;
        case SELVA_OBJECT_LONGLONG:
            RedisModule_EmitAOF(aof, "SELVA.OBJECT.SET", "sbl",
                key,
                okey->name, (size_t)okey->name_len,
                okey->emb_ll_value);
            break;
        case SELVA_OBJECT_STRING:
            RedisModule_EmitAOF(aof, "SELVA.OBJECT.SET", "sbs",
                okey->name, (size_t)okey->name_len,
                okey->value);
            break;
        default:
            RedisModule_LogIOError(aof, "warning", "Unknown type");
        }
    }
}

void SelvaObjectTypeFree(void *value) {
    struct SelvaObject *obj = (struct SelvaObject *)value;

    destroy_selva_object(obj);
}

static int SelvaObject_OnLoad(RedisModuleCtx *ctx) {
    RedisModuleTypeMethods tm = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = SelvaObjectTypeRDBLoad,
        .rdb_save = SelvaObjectTypeRDBSave,
#if 0
        .aof_rewrite = SelvaObjectTypeAOFRewrite,
#endif
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
        RedisModule_CreateCommand(ctx, "selva.object.getall", SelvaObject_GetAllCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
#if 0
        RedisModule_CreateCommand(ctx, "selva.object.getrange", SelvaObject_GetRangeCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.incrby", SelvaObject_IncrbyCommand, "write", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.incrbydouble", SelvaObject_IncrbyDoubleCommand, "write", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.keys", SelvaObject_KeysCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.len", SelvaObject_LenCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.mget", SelvaObject_MgetCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.mset", SelvaObject_MsetCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.scan", SelvaObject_ScanCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
#endif
        RedisModule_CreateCommand(ctx, "selva.object.set", SelvaObject_SetCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
#if 0
        RedisModule_CreateCommand(ctx, "selva.object.setnx", SelvaObject_SetNXCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.strlen", SelvaObject_StrlenCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
#endif
        RedisModule_CreateCommand(ctx, "selva.object.type", SelvaObject_TypeCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR
        /*RedisModule_CreateCommand(ctx, "selva.object.vals", SelvaObject_ValsCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR*/) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(SelvaObject_OnLoad);
