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

static ssize_t ref2rms(RedisModuleCtx *ctx, int8_t type, const char *s, RedisModuleString **out) {
    size_t len;
    RedisModuleString *rms;

    switch (type) {
    case SELVA_MODIFY_OP_SET_TYPE_CHAR:
        len = strlen(s);
        break;
    case SELVA_MODIFY_OP_SET_TYPE_REFERENCE:
        len = strnlen(s, SELVA_NODE_ID_SIZE);
        if (len == 0) {
            return SELVA_EINVAL;
        }
        break;
    default:
        return SELVA_EINTYPE;
    }

    rms = RedisModule_CreateString(ctx, s, len);
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
    RedisModuleString *key_name;

    key_name = RedisModule_CreateString(ctx, HIERARCHY_DEFAULT_KEY, sizeof(HIERARCHY_DEFAULT_KEY) - 1);
    if (!key_name) {
        return SELVA_ENOMEM;
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

static void selva_set_defer_alias_change_events(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        struct SelvaSet *aliases) {
    struct SelvaSetElement *el;

    SELVA_SET_RMS_FOREACH(el, aliases) {
        RedisModuleString *alias_name = el->value_rms;

        Selva_Subscriptions_DeferAliasChangeEvents(ctx, hierarchy, alias_name);
    }
}

/**
 * Add all values from value_ptr to the set in obj.field.
 */
static int add_set_values(
    RedisModuleCtx *ctx,
    SelvaModify_Hierarchy *hierarchy,
    RedisModuleKey *alias_key,
    struct SelvaObject *obj,
    RedisModuleString *id,
    RedisModuleString *field,
    char *value_ptr,
    size_t value_len,
    int8_t type
) {
    char *ptr = value_ptr;

    if (type == SELVA_MODIFY_OP_SET_TYPE_CHAR ||
        type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) {
        for (size_t i = 0; i < value_len; ) {
            int err;
            RedisModuleString *ref;
            const ssize_t part_len = ref2rms(ctx, type, ptr, &ref);

            if (part_len < 0) {
                return SELVA_EINVAL;
            }

            /* Add to the global aliases hash. */
            if (alias_key) {
                Selva_NodeId node_id;
                TO_STR(id);

                Selva_NodeIdCpy(node_id, id_str);
                Selva_Subscriptions_DeferAliasChangeEvents(ctx, hierarchy, ref);

                update_alias(ctx, alias_key, id, ref);
            }

            /* Add to the node object. */
            err = SelvaObject_AddStringSet(obj, field, ref);
            if (err == 0) {
                RedisModule_RetainString(ctx, ref);
            } else if (err != SELVA_EEXIST) {
                /* TODO Handle error */
                if (alias_key) {
                    fprintf(stderr, "%s: Alias update failed partially\n", __FILE__);
                } else {
                    fprintf(stderr, "%s: String set field update failed\n", __FILE__);
                }
            }

            /* +1 to skip the NUL if cstring */
            const size_t skip_off = type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE ? SELVA_NODE_ID_SIZE : (size_t)part_len + (type == SELVA_MODIFY_OP_SET_TYPE_CHAR);
            if (skip_off == 0) {
                return SELVA_EINVAL;
            }

            ptr += skip_off;
            i += skip_off;
        }
    } else if (type == SELVA_MODIFY_OP_SET_TYPE_DOUBLE ||
               type == SELVA_MODIFY_OP_SET_TYPE_LONG_LONG) {
        for (size_t i = 0; i < value_len; ) {
            int err;
            size_t part_len;

            /*
             * We want to be absolutely sure that we don't hit alignment aborts
             * on any architecture even if the received data is unaligned, hence
             * we use memcpy here.
             */
            if (type == SELVA_MODIFY_OP_SET_TYPE_DOUBLE) {
                double v;

                part_len = sizeof(double);
                memcpy(&v, ptr, part_len);
                err = SelvaObject_AddDoubleSet(obj, field, v);
            } else {
                long long v;

                part_len = sizeof(long long);
                memcpy(&v, ptr, part_len);
                err = SelvaObject_AddLongLongSet(obj, field, v);
            }
            if (err && err != SELVA_EEXIST) {
                /* TODO Handle error */
                fprintf(stderr, "%s: Double set field update failed\n", __FILE__);
            }

            const size_t skip_off = part_len;
            if (skip_off == 0) {
                return SELVA_EINVAL;
            }

            ptr += skip_off;
            i += skip_off;
        }
    } else {
        return SELVA_EINTYPE;
    }

    return 0;
}

static int del_set_values(
    RedisModuleCtx *ctx,
    RedisModuleKey *alias_key,
    struct SelvaObject *obj,
    RedisModuleString *field,
    char *value_ptr,
    size_t value_len,
    int8_t type
) {
    char *ptr = value_ptr;

    if (type == SELVA_MODIFY_OP_SET_TYPE_CHAR ||
        type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) {
        for (size_t i = 0; i < value_len; ) {
            RedisModuleString *ref;
            const ssize_t part_len = ref2rms(ctx, type, ptr, &ref);

            if (part_len < 0) {
                return SELVA_EINVAL;
            }

            /* Remove from the node object. */
            SelvaObject_RemStringSet(obj, field, ref);

            /* Remove from the global aliases hash. */
            if (alias_key) {
                RedisModule_HashSet(alias_key, REDISMODULE_HASH_NONE, ref, REDISMODULE_HASH_DELETE, NULL);
            }

            /* +1 to skip the NUL if cstring */
            const size_t skip_off = type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE ? SELVA_NODE_ID_SIZE : (size_t)part_len + (type == SELVA_MODIFY_OP_SET_TYPE_CHAR);
            if (skip_off == 0) {
                return SELVA_EINVAL;
            }

            ptr += skip_off;
            i += skip_off;
        }
    } else if (type == SELVA_MODIFY_OP_SET_TYPE_DOUBLE ||
               type == SELVA_MODIFY_OP_SET_TYPE_LONG_LONG) {
        for (size_t i = 0; i < value_len; ) {
            int err;
            size_t part_len;

            /*
             * We want to be absolutely sure that we don't hit alignment aborts
             * on any architecture even if the received data is unaligned, hence
             * we use memcpy here.
             */
            if (type == SELVA_MODIFY_OP_SET_TYPE_DOUBLE) {
                double v;

                part_len = sizeof(double);
                memcpy(&v, ptr, part_len);
                err = SelvaObject_RemDoubleSet(obj, field, v);
            } else {
                long long v;

                part_len = sizeof(long long);
                memcpy(&v, ptr, part_len);
                err = SelvaObject_RemLongLongSet(obj, field, v);
            }
            if (err && err != SELVA_EEXIST) {
                /* TODO Handle error */
                fprintf(stderr, "%s: Double set field update failed\n", __FILE__);
            }

            const size_t skip_off = part_len;
            ptr += skip_off;
            i += skip_off;
        }
    } else {
        return SELVA_EINTYPE;
    }

    return 0;
}

static int update_set(
    RedisModuleCtx *ctx,
    SelvaModify_Hierarchy *hierarchy,
    struct SelvaObject *obj,
    RedisModuleString *id,
    RedisModuleString *field,
    struct SelvaModify_OpSet *setOpts
) {
    TO_STR(id, field);
    RedisModuleKey *alias_key = NULL;

    if (!strcmp(field_str, "aliases")) {
        alias_key = open_aliases_key(ctx);
        if (!alias_key) {
            fprintf(stderr, "%s: Unable to open aliases\n", __FILE__);
            return SELVA_ENOENT;
        }
    }

    if (setOpts->$value_len > 0) {
        int err;

        if (alias_key) {
            struct SelvaSet *node_aliases = SelvaObject_GetSet(obj, field);

            if (node_aliases && node_aliases->type == SELVA_SET_TYPE_RMSTRING) {
                selva_set_defer_alias_change_events(ctx, hierarchy, node_aliases);
                delete_aliases(alias_key, node_aliases);
            }
        }
        err = SelvaObject_DelKey(obj, field);
        if (err && err != SELVA_ENOENT) {
            fprintf(stderr, "%s: Unable to remove a set \"%s:%s\"\n", __FILE__, id_str, field_str);
            return SELVA_ENOENT; /* TODO is this the right error. */
        }

        /*
         * Set new values.
         */
        err = add_set_values(ctx, hierarchy, alias_key, obj, id, field, setOpts->$value, setOpts->$value_len, setOpts->op_set_type);
        if (err) {
            return err;
        }
    } else {
        if (setOpts->$add_len > 0) {
            int err;

            err = add_set_values(ctx, hierarchy, alias_key, obj, id, field, setOpts->$add, setOpts->$add_len, setOpts->op_set_type);
            if (err) {
                return err;
            }
        }

        if (setOpts->$delete_len > 0) {
            int err;

            err = del_set_values(ctx, alias_key, obj, field, setOpts->$delete,setOpts->$delete_len, setOpts->op_set_type);
            if (err) {
                return err;
            }
        }
    }

    return 0;
}

int SelvaModify_ModifySet(
    RedisModuleCtx *ctx,
    SelvaModify_Hierarchy *hierarchy,
    struct SelvaObject *obj,
    RedisModuleString *id,
    RedisModuleString *field,
    struct SelvaModify_OpSet *setOpts
) {
    const int is_reference = setOpts->op_set_type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE;
    TO_STR(id, field);

    if (setOpts->delete_all) {
        int err;

        if (is_reference && !strcmp(field_str, "children")) {
            Selva_NodeId node_id;

            Selva_NodeIdCpy(node_id, id_str);
            err = SelvaModify_DelHierarchyChildren(hierarchy, node_id);
        } else if (!strcmp(field_str, "parents")) {
            Selva_NodeId node_id;

            Selva_NodeIdCpy(node_id, id_str);
            err = SelvaModify_DelHierarchyParents(hierarchy, node_id);
        } else {
            /*
             * First we need to delete the aliases of this node from the
             * ___selva_aliases hash.
             */
            if (!strcmp(field_str, "aliases")) {
                RedisModuleKey *alias_key;
                struct SelvaSet *node_aliases;

                alias_key = open_aliases_key(ctx);
                if (!alias_key) {
                    fprintf(stderr, "%s: Unable to open aliases\n", __FILE__);
                    return SELVA_ENOENT;
                }

                node_aliases = SelvaObject_GetSet(obj, field);
                if (node_aliases) {
                    selva_set_defer_alias_change_events(ctx, hierarchy, node_aliases);
                    delete_aliases(alias_key, node_aliases);
                }
            }

            err = SelvaObject_DelKey(obj, field);
        }

        if (err != SELVA_ENOENT) {
            return err;
        }
    }

    if (setOpts->op_set_type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE &&
        (!strcmp(field_str, "children") || !strcmp(field_str, "parents"))) {
        Selva_NodeId node_id;

        Selva_NodeIdCpy(node_id, id_str);
        return update_hierarchy(ctx, hierarchy, node_id, field_str, setOpts);
    } else {
        return update_set(ctx, hierarchy, obj, id, field, setOpts);
    }
}

void SelvaModify_ModifyIncrement(
    struct SelvaObject *obj,
    RedisModuleString *field,
    enum SelvaObjectType old_type,
    struct SelvaModify_OpIncrement *incrementOpts
) {
    int64_t new = incrementOpts->$default;

    if (old_type == SELVA_OBJECT_LONGLONG) {
        long long old;

        if (!SelvaObject_GetLongLong(obj, field, &old)) {
            new = old + incrementOpts->$increment;
        }
    }

    (void)SelvaObject_SetLongLong(obj, field, new);
}

void SelvaModify_ModifyIncrementDouble(
    RedisModuleCtx *ctx,
    struct SelvaObject *obj,
    RedisModuleString *field,
    enum SelvaObjectType old_type,
    struct SelvaModify_OpIncrementDouble *incrementOpts
) {
    double new = incrementOpts->$default;

    if (old_type == SELVA_OBJECT_DOUBLE) {
        double old;

        if (!SelvaObject_GetDouble(obj, field, &old)) {
            new = old + incrementOpts->$increment;
        }
    }

    (void)SelvaObject_SetDouble(obj, field, new);
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
