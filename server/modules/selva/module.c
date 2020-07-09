#include <math.h>

#include "cdefs.h"
#include "redismodule.h"
#include "rmutil/util.h"
#include "rmutil/strings.h"
#include "rmutil/test_util.h"

#include "id/id.h"
#include "modify/modify.h"
#include "modify/async_task.h"
#include "modify/hierarchy.h"

#define TO_STR_1(_var) \
    size_t _var##_len; \
    const char * _var##_str = RedisModule_StringPtrLen(_var, & _var##_len);

#define TO_STR_2(_var, ...) \
    TO_STR_1(_var) \
    TO_STR_1(__VA_ARGS__)

#define TO_STR_3(_var, ...) \
    TO_STR_1(_var) \
    TO_STR_2(__VA_ARGS__)

#define TO_STR_4(_var, ...) \
    TO_STR_1(_var) \
    TO_STR_3(__VA_ARGS__)

#define TO_STR_5(_var, ...) \
    TO_STR_1(_var) \
    TO_STR_5(__VA_ARGS__)

#define TO_STR_6(_var, ...) \
    TO_STR_1(_var) \
    TO_STR_6(__VA_ARGS__)

#define TO_STR(...) \
        CONCATENATE(TO_STR_, UTIL_NARG(__VA_ARGS__))(__VA_ARGS__)

int SelvaCommand_GenId(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // init auto memory for created strings
    RedisModule_AutoMemory(ctx);
    Selva_NodeId hash_str;

    if (argc > 2) {
        return RedisModule_WrongArity(ctx);
    }

    SelvaId_GenId("", hash_str);

    RedisModuleString *reply = RedisModule_CreateString(ctx, hash_str, sizeof(hash_str));
    RedisModule_ReplyWithString(ctx, reply);
    return REDISMODULE_OK;
}

int SelvaCommand_Flurpy(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    REDISMODULE_NOT_USED(argv);
    REDISMODULE_NOT_USED(argc);

    // init auto memory for created strings
    RedisModule_AutoMemory(ctx);

    RedisModuleString *keyStr =
            RedisModule_CreateString(ctx, "flurpypants", strlen("flurpypants"));
    RedisModuleString *val = RedisModule_CreateString(ctx, "hallo", strlen("hallo"));
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyStr, REDISMODULE_WRITE);
    for (int i = 0; i < 10000; i++) {
        RedisModule_StringSet(key, val);
        // RedisModuleCallReply *r = RedisModule_Call(ctx, "publish", "x", "y");
    }

    RedisModule_CloseKey(key);
    RedisModuleString *reply = RedisModule_CreateString(ctx, "hallo", strlen("hallo"));
    RedisModule_ReplyWithString(ctx, reply);
    return REDISMODULE_OK;
}

// TODO: clean this up
// id, type, key, value [, ... type, key, value]]
int SelvaCommand_Modify(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err = REDISMODULE_OK;

    if (argc < 2 || (argc - 2) % 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *id = argv[1];
    size_t id_len;
    const char *id_str = RedisModule_StringPtrLen(id, &id_len);

    RedisModuleKey *id_key = RedisModule_OpenKey(ctx, id, REDISMODULE_WRITE);

    /*
     * If this is a new node we need to create a hierarchy node for it.
     * TODO It should be possible to skip this
     */
    if (RedisModule_KeyType(id_key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModuleString *key_name;
        SelvaModify_Hierarchy *hierarchy;
        Selva_NodeId nodeId;

        key_name = RedisModule_CreateString(ctx, HIERARCHY_DEFAULT_KEY, sizeof(HIERARCHY_DEFAULT_KEY) - 1);
        hierarchy = SelvaModify_OpenHierarchyKey(ctx, key_name);
        if (!hierarchy) {
            err = REDISMODULE_ERR;
            goto out;
        }


        memset(nodeId, '\0', SELVA_NODE_ID_SIZE);
        memcpy(nodeId, id_str, min(id_len, SELVA_NODE_ID_SIZE));

        int err = SelvaModify_SetHierarchy(hierarchy, nodeId, 0, NULL, 0, NULL);
        if (err) {
            RedisModule_ReplyWithError(ctx, hierarchyStrError[-err]);
            err = REDISMODULE_ERR;
            goto out;
        }
    }

    for (int i = 2; i < argc; i += 3) {
        bool publish = true;
        RedisModuleString *type = argv[i];
        RedisModuleString *field = argv[i + 1];
        RedisModuleString *value = argv[i + 2];

        TO_STR(type, field, value);

        size_t current_value_len = 0;
        RedisModuleString *current_value;
        RedisModule_HashGet(id_key, REDISMODULE_HASH_NONE, field, &current_value, NULL);
        const char *current_value_str = NULL;

        if (current_value != NULL) {
            current_value_str = RedisModule_StringPtrLen(current_value, &current_value_len);
        }

        char type_code = type_str[0];

        if (type_code != SELVA_MODIFY_ARG_OP_INCREMENT && type_code != SELVA_MODIFY_ARG_OP_SET &&
                current_value && current_value_len == value_len &&
                !memcmp(current_value, value, min(current_value_len, value_len))) {
            // printf("Current value is equal to the specified value for key %s and value %s\n", field_str,
            //              value_str);
            continue;
        }

        if (type_code == SELVA_MODIFY_ARG_OP_INCREMENT) {
            struct SelvaModify_OpIncrement *incrementOpts = (struct SelvaModify_OpIncrement *)value_str;
            SelvaModify_ModifyIncrement(ctx, id_key, id_str, id_len, field, field_str, field_len,
                    current_value, current_value_str, current_value_len, incrementOpts);
        } else if (type_code == SELVA_MODIFY_ARG_OP_SET) {
            struct SelvaModify_OpSet *setOpts = (struct SelvaModify_OpSet *)value_str;
            SelvaModify_OpSet_align(setOpts);

            err = SelvaModify_ModifySet(ctx, id_key, id_str, id_len, field, field_str, field_len, setOpts);
            if (err) {
                goto out;
            }
        } else {
            if (type_code == SELVA_MODIFY_ARG_INDEXED_VALUE ||
                    type_code == SELVA_MODIFY_ARG_DEFAULT_INDEXED) {
                SelvaModify_Index(id_str, id_len, field_str, field_len, value_str, value_len);
            }

            if (type_code == SELVA_MODIFY_ARG_DEFAULT || type_code == SELVA_MODIFY_ARG_DEFAULT_INDEXED) {
                if (current_value != NULL) {
                    publish = false;
                } else {
                    RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, value, NULL);
                }
            } else if (type_code == SELVA_MODIFY_ARG_VALUE) {
                RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, value, NULL);
            } else {
                fprintf(stderr, "Invalid type: \"%c\"", type_code);
            }
        }

        if (publish) {
            SelvaModify_Publish(id_str, id_len, field_str, field_len);
        }
    }

    RedisModule_ReplyWithString(ctx, id);
out:
    RedisModule_CloseKey(id_key);

    return err;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {
    int Hierarchy_OnLoad(RedisModuleCtx *ctx);

    // Register the module itself
    if (RedisModule_Init(ctx, "selva", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "selva.id", SelvaCommand_GenId, "readonly", 1, 1, 1) ==
            REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "selva.modify", SelvaCommand_Modify, "readonly", 1, 1, 1) ==
            REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "selva.flurpypants", SelvaCommand_Flurpy, "readonly", 1, 1,
                                                                1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return Hierarchy_OnLoad(ctx);
}
