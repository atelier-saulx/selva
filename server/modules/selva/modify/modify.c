#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "redismodule.h"

#include "hierarchy.h"
#include "modify.h"

const char HIERARCHY_KEY_NAME[] = "___selva_hierarchy";

static int update_hierarchy(
    RedisModuleCtx *ctx,
    RedisModuleKey *id_key,
    const char *id_str,
    size_t id_len,
    RedisModuleString *field,
    const char *field_str,
    size_t field_len,
    struct SelvaModify_OpSet *setOpts
) {
    RedisModuleString *key_name = RedisModule_CreateString(ctx, HIERARCHY_KEY_NAME, sizeof(HIERARCHY_KEY_NAME) - 1);
    RedisModuleKey *key = RedisModule_OpenKey(ctx, key_name, REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != HierarchyType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    /* Create an empty value object if the key is currently empty. */
    SelvaModify_Hierarchy *hierarchy;
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        hierarchy = SelvaModify_NewHierarchy();
        RedisModule_ModuleTypeSetValue(key, HierarchyType, hierarchy);
    } else {
        hierarchy = RedisModule_ModuleTypeGetValue(key);
    }

    /*
     * If the field starts with 'p' we assume "parents"; Otherwise "children".
     * No other field can modify the hierarchy.
     */
    int isFieldParents = field_str[0] == 'p';

    int err = 0;
    if (setOpts->$value_len > 0) {
        size_t nr_nodes = setOpts->$value_len / SELVA_NODE_ID_SIZE;

        if (isFieldParents) { /* parents */
          err = SelvaModify_SetHierarchy(hierarchy, id_str,
                  nr_nodes, (const Selva_NodeId *)setOpts->$value,
                  0, NULL);
        } else { /* children */
          err = SelvaModify_SetHierarchy(hierarchy, id_str,
                  0, NULL,
                  nr_nodes, (const Selva_NodeId *)setOpts->$value);
        }
    } else {
        if (setOpts->$add_len > 0) {
            size_t nr_nodes = setOpts->$add_len / SELVA_NODE_ID_SIZE;

            if (isFieldParents) { /* parents */
              err = SelvaModify_AddHierarchy(hierarchy, id_str,
                      nr_nodes, (const Selva_NodeId *)setOpts->$add,
                      0, NULL);
            } else { /* children */
              err = SelvaModify_AddHierarchy(hierarchy, id_str,
                      0, NULL,
                      nr_nodes, (const Selva_NodeId *)setOpts->$add);
            }
        }
        if (setOpts->$delete_len > 0) {
            size_t nr_nodes = setOpts->$add_len / SELVA_NODE_ID_SIZE;

            if (isFieldParents) { /* parents */
                err = SelvaModify_DelHierarchy(hierarchy, id_str,
                        nr_nodes, (const Selva_NodeId *)setOpts->$delete,
                        0, NULL);
            } else { /* children */
                err = SelvaModify_DelHierarchy(hierarchy, id_str,
                        0, NULL,
                        nr_nodes, (const Selva_NodeId *)setOpts->$delete);
            }
        }

    }

    RedisModule_CloseKey(key);

    if (err) {
        RedisModule_ReplyWithError(ctx, hierarchyStrError[-err]);
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}

static int update_zset(
    RedisModuleCtx *ctx,
    RedisModuleKey *id_key,
    const char *id_str,
    size_t id_len,
    RedisModuleString *field,
    const char *field_str,
    size_t field_len,
    struct SelvaModify_OpSet *setOpts
) {
    // add in the hash that it's a set/references field
    RedisModuleString *set_field_identifier = RedisModule_CreateString(ctx, "___selva_$set", 13);
    RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, set_field_identifier, NULL);

    RedisModuleString *set_key_name = RedisModule_CreateStringPrintf(ctx, "%.*s%c%.*s", id_len, id_str, '.', field_len, field_str);
    RedisModuleKey *set_key = RedisModule_OpenKey(ctx, set_key_name, REDISMODULE_WRITE);

    if (!set_key) {
        return REDISMODULE_ERR;
    }
    if (setOpts->$value_len > 0) {
        RedisModule_UnlinkKey(set_key);

        char *ptr = setOpts->$value;
        for (size_t i = 0; i < setOpts->$value_len; ) {
            unsigned long part_len = strlen(ptr);

            RedisModuleString *ref = RedisModule_CreateString(ctx, ptr, part_len);
            RedisModule_ZsetAdd(set_key, 0, ref, NULL);

            // +1 to skip the nullbyte
            ptr += part_len + 1;
            i += part_len + 1;
        }
    } else {
        if (setOpts->$add_len > 0) {
            char *ptr = setOpts->$add;
            for (size_t i = 0; i < setOpts->$add_len; ) {
                unsigned long part_len = strlen(ptr);

                RedisModuleString *ref = RedisModule_CreateString(ctx, ptr, part_len);
                RedisModule_ZsetAdd(set_key, 0, ref, NULL);

                // +1 to skip the nullbyte
                ptr += part_len + 1;
                i += part_len + 1;
            }
        }

        if (setOpts->$delete_len > 0) {
            char *ptr = setOpts->$delete;
            for (size_t i = 0; i < setOpts->$delete_len; ) {
                unsigned long part_len = strlen(ptr);

                RedisModuleString *ref = RedisModule_CreateString(ctx, ptr, part_len);
                RedisModule_ZsetRem(set_key, ref, NULL);

                // +1 to skip the nullbyte
                ptr += part_len + 1;
                i += part_len + 1;
            }
        }
    }

    RedisModule_CloseKey(set_key);
    return REDISMODULE_OK;
}

int SelvaModify_ModifySet(
    RedisModuleCtx *ctx,
    RedisModuleKey *id_key,
    const char *id_str,
    size_t id_len,
    RedisModuleString *field,
    const char *field_str,
    size_t field_len,
    struct SelvaModify_OpSet *setOpts
) {
    if (setOpts->is_reference) {
        /*
         * Currently only parents and children fields support references (using
         * hierarchy) and we assume the field is either of those.
         */
        return update_hierarchy(ctx, id_key, id_str, id_len, field, field_str, field_len, setOpts);
    } else {
        return update_zset(ctx, id_key, id_str, id_len, field, field_str, field_len, setOpts);
    }
}

void SelvaModify_ModifyIncrement(
    RedisModuleCtx *ctx,
    RedisModuleKey *id_key,
    const char *id_str,
    size_t id_len,
    RedisModuleString *field,
    const char *field_str,
    size_t field_len,
    RedisModuleString *current_value,
    const char *current_value_str,
    size_t current_value_len,
    struct SelvaModify_OpIncrement *incrementOpts
) {
    int num = current_value == NULL
        ? incrementOpts->$default
        : strtol(current_value_str, NULL, SELVA_NODE_ID_SIZE);
    num += incrementOpts->$increment;

    int num_str_size = (int)ceil(log10(num));
    char increment_str[num_str_size];
    sprintf(increment_str, "%d", num);

    RedisModuleString *increment =
        RedisModule_CreateString(ctx, increment_str, num_str_size);
    RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, increment, NULL);

    if (incrementOpts->index) {
        SelvaModify_Index(id_str, id_len, field_str, field_len, increment_str, num_str_size);
    }
}
