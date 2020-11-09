#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "redismodule.h"
#include "cdefs.h"
#include "errors.h"
#include "hierarchy.h"
#include "modify.h"
#include "selva_node.h"
#include "selva_object.h"
#include "selva_set.h"

static ssize_t ref2rms(RedisModuleCtx *ctx, struct SelvaModify_OpSet * restrict setOpts, const char *s, RedisModuleString **out) {
    size_t len = setOpts->is_reference ? strnlen(s, SELVA_NODE_ID_SIZE) : strlen(s);
    RedisModuleString *rms = RedisModule_CreateString(ctx, s, len);

    if (!rms) {
        return SELVA_ENOMEM;
    }
    *out = rms;

    return len;
}

static int update_hierarchy(
    RedisModuleCtx *ctx,
    SelvaModify_Hierarchy *hierarchy,
    Selva_NodeId node_id,
    const char *field_str,
    struct SelvaModify_OpSet *setOpts
) {
    RedisModuleString *key_name = RedisModule_CreateString(ctx, HIERARCHY_DEFAULT_KEY, sizeof(HIERARCHY_DEFAULT_KEY) - 1);
    hierarchy = SelvaModify_OpenHierarchy(ctx, key_name, REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        fprintf(stderr, "%s: Failed to open hierarchy\n", __FILE__);
        return 0; /* RFE Not sure if it's ok to return 0 here */
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
            err = SelvaModify_SetHierarchyParents(ctx, hierarchy, node_id,
                    nr_nodes, (const Selva_NodeId *)setOpts->$value);
        } else { /* children */
            err = SelvaModify_SetHierarchyChildren(ctx, hierarchy, node_id,
                    nr_nodes, (const Selva_NodeId *)setOpts->$value);
        }
    } else {
        if (setOpts->$add_len > 0) {
            size_t nr_nodes = setOpts->$add_len / SELVA_NODE_ID_SIZE;

            if (isFieldParents) { /* parents */
              err = SelvaModify_AddHierarchy(ctx, hierarchy, node_id,
                      nr_nodes, (const Selva_NodeId *)setOpts->$add,
                      0, NULL);
            } else { /* children */
              err = SelvaModify_AddHierarchy(ctx, hierarchy, node_id,
                      0, NULL,
                      nr_nodes, (const Selva_NodeId *)setOpts->$add);
            }
        }
        if (setOpts->$delete_len > 0) {
            size_t nr_nodes = setOpts->$delete_len / SELVA_NODE_ID_SIZE;

            if (isFieldParents) { /* parents */
                err = SelvaModify_DelHierarchy(hierarchy, node_id,
                        nr_nodes, (const Selva_NodeId *)setOpts->$delete,
                        0, NULL);
            } else { /* children */
                err = SelvaModify_DelHierarchy(hierarchy, node_id,
                        0, NULL,
                        nr_nodes, (const Selva_NodeId *)setOpts->$delete);
            }
        }
    }

    return err;
}

int update_set(
    RedisModuleCtx *ctx,
    RedisModuleKey *id_key,
    RedisModuleString *id,
    RedisModuleString *field,
    struct SelvaModify_OpSet *setOpts
) {
    int err;
    TO_STR(id, field);
    RedisModuleKey *alias_key = NULL;
    struct SelvaObject *obj;

    err = SelvaObject_Key2Obj(id_key, &obj);
    if (err) {
        return err;
    }

    if (!strcmp(field_str, "aliases")) {
        alias_key = open_aliases_key(ctx);
        if (!alias_key) {
            fprintf(stderr, "%s: Unable to open aliases\n", __FILE__);
            return SELVA_ENOENT;
        }
    }

    if (setOpts->$value_len > 0) {
        if (alias_key) {
            delete_aliases(alias_key, SelvaObject_GetSet(obj, field));
        }
        err = SelvaObject_DelKey(obj, field);
        if (err && err != SELVA_ENOENT) {
            fprintf(stderr, "%s: Unable to remove a set \"%s:%s\"\n", __FILE__, id_str, field_str);
            return SELVA_ENOENT;
        }

        /*
         * Set new values.
         */
        char *ptr = setOpts->$value;
        for (size_t i = 0; i < setOpts->$value_len; ) {
            RedisModuleString *ref;
            const ssize_t part_len = ref2rms(ctx, setOpts, ptr, &ref);

            if (part_len < 0) {
                return SELVA_EINVAL;
            }

            if (alias_key) {
                update_alias(ctx, alias_key, id, ref);
            }
            (void)SelvaObject_AddSet(obj, field, ref);

            // +1 to skip the nullbyte
            const size_t skip_off = setOpts->is_reference ? SELVA_NODE_ID_SIZE : (size_t)part_len + 1;
            ptr += skip_off;
            i += skip_off;
        }
    } else {
        if (setOpts->$add_len > 0) {
            char *ptr = setOpts->$add;
            for (size_t i = 0; i < setOpts->$add_len; ) {
                RedisModuleString *ref;
                const ssize_t part_len = ref2rms(ctx, setOpts, ptr, &ref);

                if (part_len < 0) {
                    return SELVA_EINVAL;
                }

                if (alias_key) {
                    update_alias(ctx, alias_key, id, ref);
                }
                (void)SelvaObject_AddSet(obj, field, ref);

                // +1 to skip the nullbyte
                const size_t skip_off = setOpts->is_reference ? SELVA_NODE_ID_SIZE : (size_t)part_len + 1;
                ptr += skip_off;
                i += skip_off;
            }
        }

        if (setOpts->$delete_len > 0) {
            char *ptr = setOpts->$delete;
            for (size_t i = 0; i < setOpts->$delete_len; ) {
                RedisModuleString *ref;
                const ssize_t part_len = ref2rms(ctx, setOpts, ptr, &ref);

                if (part_len < 0) {
                    return SELVA_EINVAL;
                }

                SelvaObject_RemSet(obj, field, ref);

                // +1 to skip the nullbyte
                const size_t skip_off = setOpts->is_reference ? SELVA_NODE_ID_SIZE : (size_t)part_len + 1;
                ptr += skip_off;
                i += skip_off;

                if (alias_key) {
                    RedisModule_HashSet(alias_key, REDISMODULE_HASH_NONE, ref, REDISMODULE_HASH_DELETE, NULL);
                }
            }
        }
    }

    return 0;
}

int SelvaModify_ModifySet(
    RedisModuleCtx *ctx,
    SelvaModify_Hierarchy *hierarchy,
    RedisModuleKey *id_key,
    RedisModuleString *id,
    RedisModuleString *field,
    struct SelvaModify_OpSet *setOpts
) {
    TO_STR(id, field);
    Selva_NodeId node_id;

    if (setOpts->is_reference) {
        memset(node_id, '\0', SELVA_NODE_ID_SIZE);
        memcpy(node_id, id_str, min(id_len, SELVA_NODE_ID_SIZE));
    }

    if (setOpts->delete_all) {
        int err;

        if (!strcmp(field_str, "children")) {
            err = SelvaModify_DelHierarchyChildren(hierarchy, node_id);
        } else if (!strcmp(field_str, "parents")) {
            err = SelvaModify_DelHierarchyParents(hierarchy, node_id);
        } else {
            struct SelvaObject *obj;

            err = SelvaObject_Key2Obj(id_key, &obj);
            if (err) {
                return err;
            }

            /*
             * First we need to delete the aliases of this node from the
             * ___selva_aliases hash.
             */
            if (!strcmp(field_str, "aliases")) {
                RedisModuleKey *alias_key;

                alias_key = open_aliases_key(ctx);
                if (!alias_key) {
                    fprintf(stderr, "%s: Unable to open aliases\n", __FILE__);
                    return SELVA_ENOENT;
                }

                delete_aliases(alias_key, SelvaObject_GetSet(obj, field));
            }

            err = SelvaObject_DelKey(obj, field);
        }

        if (err) {
            char err_msg[80];

            snprintf(err_msg, sizeof(err_msg), "ERR Failed to delete the set: \"%s\"", field_str);
            return SELVA_EGENERAL;
        }
    }

    if (!strcmp(field_str, "children") || !strcmp(field_str, "parents")) {
        return update_hierarchy(ctx, hierarchy, node_id, field_str, setOpts);
    } else {
        return update_set(ctx, id_key, id, field, setOpts);
    }
}

void SelvaModify_ModifyIncrement(
    RedisModuleCtx *ctx,
    struct SelvaObject *obj,
    RedisModuleString *field,
    RedisModuleString *current_value,
    struct SelvaModify_OpIncrement *incrementOpts
) {
    int32_t num = incrementOpts->$default;

    if (current_value) {
        TO_STR(current_value);

        num = strtol(current_value_str, NULL, 10) + incrementOpts->$increment;
    }

    const size_t num_str_size = (int)(log10(num)) + 1;
    char increment_str[num_str_size];
    RedisModuleString *increment;

    sprintf(increment_str, "%d", num);
    increment = RedisModule_CreateString(ctx, increment_str, num_str_size);
    (void)SelvaObject_SetStr(obj, field, increment);
}

int SelvaModify_ModifyDel(
    RedisModuleCtx *ctx __unused,
    SelvaModify_Hierarchy *hierarchy,
    struct SelvaObject *obj,
    RedisModuleString *id,
    RedisModuleString *field
) {
    TO_STR(id, field);
    int err = 0;

    if (!strcmp(field_str, "children")) {
        Selva_NodeId node_id;

        memset(node_id, '\0', SELVA_NODE_ID_SIZE);
        memcpy(node_id, id_str, min(id_len, SELVA_NODE_ID_SIZE));

        err = SelvaModify_DelHierarchyChildren(hierarchy, node_id);
        if (err) {
            return err;
        }
    } else if (!strcmp(field_str, "parents")) {
        Selva_NodeId node_id;

        memset(node_id, '\0', SELVA_NODE_ID_SIZE);
        memcpy(node_id, id_str, min(id_len, SELVA_NODE_ID_SIZE));

        if (!SelvaModify_DelHierarchyParents(hierarchy, node_id)) {
            err = REDISMODULE_ERR;
        }
    } else { /* Delete a field. */
        (void)SelvaObject_DelKey(obj, field);
    }

    return err;
}
