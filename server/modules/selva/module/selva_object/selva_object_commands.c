#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "cdefs.h"
#include "typestr.h"
#include "errors.h"
#include "hierarchy.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "svector.h"
#include "selva_object.h"

static struct SelvaObject *SelvaObject_Open(RedisModuleCtx *ctx, RedisModuleString *key_name, int mode) {
    RedisModuleString *hkey_name;
    struct SelvaHierarchy *hierarchy;
    Selva_NodeId nodeId;
    const struct SelvaHierarchyNode *node;

    /*
     * Open the Redis key.
     */
    hkey_name = RedisModule_CreateString(ctx, HIERARCHY_DEFAULT_KEY, sizeof(HIERARCHY_DEFAULT_KEY) - 1);
    if (!hkey_name) {
        replyWithSelvaError(ctx, SELVA_ENOMEM);
        return NULL;
    }
    hierarchy = SelvaModify_OpenHierarchy(ctx, hkey_name, mode);
    if (!hierarchy) {
        return NULL;
    }

    Selva_RMString2NodeId(nodeId, key_name);
    node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
        return NULL;
    }

    return SelvaHierarchy_GetNodeObject(node);
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


int SelvaObject_GetCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    struct SelvaObject *obj;

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
        (void)SelvaObject_ReplyWithObjectStr(ctx, lang, obj, NULL, 0, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        return REDISMODULE_OK;
    }

    for (int i = ARGV_OKEY; i < argc; i++) {
        const RedisModuleString *okey = argv[i];
        TO_STR(okey);

        int err = 0;

        if (strstr(okey_str, ".*.")) {
            long resp_count = 0;

            RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
            err = SelvaObject_ReplyWithWildcardStr(ctx, lang, obj, okey_str, okey_len,
                                                   &resp_count, -1,
                                                   SELVA_OBJECT_REPLY_SPLICE_FLAG | SELVA_OBJECT_REPLY_BINUMF_FLAG);
            if (err == SELVA_ENOENT) {
                /* Keep looking. */
                RedisModule_ReplySetArrayLength(ctx, resp_count);
                continue;
            } else if (err) {
                replyWithSelvaErrorf(ctx, err, "Wildcard failed");
                RedisModule_ReplySetArrayLength(ctx, resp_count + 1);

                return REDISMODULE_OK;
            }

            RedisModule_ReplySetArrayLength(ctx, resp_count);
        } else {
            err = SelvaObject_ReplyWithObjectStr(ctx, lang, obj, okey_str, okey_len, SELVA_OBJECT_REPLY_BINUMF_FLAG);
            if (err == SELVA_ENOENT) {
                /* Keep looking. */
                continue;
            } else if (err) {
                return replyWithSelvaErrorf(ctx, err, "get_key");
            }
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

    const int ARGV_KEY = 1;
    const int ARGV_OKEY = 2;

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return REDISMODULE_OK;
    }

    const RedisModuleString *okey = argv[ARGV_OKEY];
    enum SelvaObjectType type;
    const char *type_str;
    size_t type_len;

    type = SelvaObject_GetType(obj, okey);
    if (type == SELVA_OBJECT_NULL) {
        return replyWithSelvaErrorf(ctx, SELVA_ENOENT, "Field not found");
    }

    type_str = SelvaObject_Type2String(type, &type_len);
    if (!type_str) {
        return replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "invalid key type %d", (int)type);
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    RedisModule_ReplyWithStringBuffer(ctx, type_str, type_len);

    if (type == SELVA_OBJECT_ARRAY) {
        enum SelvaObjectType subtype = SELVA_OBJECT_NULL;
        const char *subtype_str;
        size_t subtype_len;

        /*
         * TODO It would be nicer if we wouldn't need to look for the subtype
         * separately.
         */
        (void)SelvaObject_GetArray(obj, okey, &subtype, NULL);
        subtype_str = SelvaObject_Type2String(subtype, &subtype_len);
        if (subtype_str) {
            RedisModule_ReplyWithStringBuffer(ctx, subtype_str, subtype_len);
        } else {
            replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "invalid key subtype %d", (int)subtype);
        }

        RedisModule_ReplySetArrayLength(ctx, 2);
    } else if (type == SELVA_OBJECT_SET) {
        const struct SelvaSet *set;

        set = SelvaObject_GetSet(obj, okey);
        if (set) {
            switch (set->type) {
            case SELVA_SET_TYPE_RMSTRING:
                RedisModule_ReplyWithSimpleString(ctx, "string");
                break;
            case SELVA_SET_TYPE_DOUBLE:
                RedisModule_ReplyWithSimpleString(ctx, "double");
                break;
            case SELVA_SET_TYPE_LONGLONG:
                RedisModule_ReplyWithSimpleString(ctx, "long long");
                break;
            case SELVA_SET_TYPE_NODEID:
                RedisModule_ReplyWithSimpleString(ctx, "nodeId");
                break;
            default:
                replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "invalid set type %d", (int)set->type);
                break;
            }
        } else {
            /* Technically ENOENT but we already found the key once. */
            replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "invalid set key");
        }

        RedisModule_ReplySetArrayLength(ctx, 2);
    } else {
        RedisModule_ReplySetArrayLength(ctx, 1);
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

    const ssize_t len = SelvaObject_Len(obj, argc == 2 ? NULL : argv[ARGV_OKEY]);
    if (len < 0) {
        int err = (int)len;

        if (err == SELVA_EINTYPE) {
            return replyWithSelvaErrorf(ctx, SELVA_EINTYPE, "key type not supported");
        } else {
            return replyWithSelvaError(ctx, err);
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
    err = SelvaObject_GetUserMeta(obj, argv[ARGV_OKEY], &user_meta);
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

    const RedisModuleString *mval = argv[ARGV_MVAL];
    TO_STR(mval);

    if (mval_len < sizeof(SelvaObjectMeta_t)) {
        return replyWithSelvaErrorf(ctx, SELVA_EINTYPE,"Expected: %s", typeof_str(user_meta));
    }

    obj = SelvaObject_Open(ctx, argv[ARGV_KEY], REDISMODULE_READ);
    if (!obj) {
        return REDISMODULE_OK;
    }

    memcpy(&user_meta, mval_str, sizeof(SelvaObjectMeta_t));
    err = SelvaObject_SetUserMeta(obj, argv[ARGV_OKEY], user_meta, NULL);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Failed to set key metadata");
    }

    return RedisModule_ReplyWithLongLong(ctx, 1);
}

static int SelvaObject_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.object.del", SelvaObject_DelCommand, "write", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.exists", SelvaObject_ExistsCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.object.get", SelvaObject_GetCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR ||
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
