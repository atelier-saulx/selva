#include <assert.h>
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
#include "comparator.h"

static ssize_t string2rms(RedisModuleCtx *ctx, int8_t type, const char *s, RedisModuleString **out) {
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
    /*
     * If the field starts with 'p' we assume "parents"; Otherwise "children".
     * No other field can modify the hierarchy.
     */
    const int isFieldParents = field_str[0] == 'p';

    if (setOpts->$value_len > 0) {
        const size_t nr_nodes = setOpts->$value_len / SELVA_NODE_ID_SIZE;
        int err = 0;

        if (setOpts->$value_len % SELVA_NODE_ID_SIZE) {
            return SELVA_EINVAL;
        }

        if (isFieldParents) { /* parents */
#if 0
            fprintf(stderr, "%s:%d: Set parents of %.*s nr_nodes: %zu ",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node_id,
                    nr_nodes);
            for (size_t i = 0; i < nr_nodes; i++) {
                fprintf(stderr, "%.*s, ", (int)SELVA_NODE_ID_SIZE, ((const Selva_NodeId *)setOpts->$value)[i]);

            }
            fprintf(stderr, "\n");
#endif

            err = SelvaModify_SetHierarchyParents(ctx, hierarchy, node_id,
                    nr_nodes, (const Selva_NodeId *)setOpts->$value);
        } else { /* children */
#if 0
            fprintf(stderr, "%s:%d: Set children of %.*s nr_nodes: %zu\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node_id,
                    nr_nodes);
            for (size_t i = 0; i < nr_nodes; i++) {
                fprintf(stderr, "%.*s, ", (int)SELVA_NODE_ID_SIZE, ((const Selva_NodeId *)setOpts->$value)[i]);

            }
            fprintf(stderr, "\n");
#endif

            err = SelvaModify_SetHierarchyChildren(ctx, hierarchy, node_id,
                    nr_nodes, (const Selva_NodeId *)setOpts->$value);
        }

        return err;
    } else {
        int err, res = 0;

        if (setOpts->$add_len % SELVA_NODE_ID_SIZE ||
            setOpts->$delete_len % SELVA_NODE_ID_SIZE) {
            return SELVA_EINVAL;
        }

        if (setOpts->$add_len > 0) {
            const size_t nr_nodes = setOpts->$add_len / SELVA_NODE_ID_SIZE;

            if (isFieldParents) { /* parents */
#if 0
                fprintf(stderr, "%s:%d: Add to parents of %.*s nr_nodes: %zu\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, node_id,
                        nr_nodes);
#endif

                err = SelvaModify_AddHierarchy(ctx, hierarchy, node_id,
                        nr_nodes, (const Selva_NodeId *)setOpts->$add,
                        0, NULL);
            } else { /* children */
#if 0
                fprintf(stderr, "%s:%d: Add to children of %.*s nr_nodes: %zu\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, node_id,
                        nr_nodes);
#endif

                err = SelvaModify_AddHierarchy(ctx, hierarchy, node_id,
                        0, NULL,
                        nr_nodes, (const Selva_NodeId *)setOpts->$add);
            }
            if (err < 0) {
                return err;
            }
            res += err;
        }
        if (setOpts->$delete_len > 0) {
            const size_t nr_nodes = setOpts->$delete_len / SELVA_NODE_ID_SIZE;

            if (isFieldParents) { /* parents */
                err = SelvaModify_DelHierarchy(ctx, hierarchy, node_id,
                        nr_nodes, (const Selva_NodeId *)setOpts->$delete,
                        0, NULL);
            } else { /* children */
                err = SelvaModify_DelHierarchy(ctx, hierarchy, node_id,
                        0, NULL,
                        nr_nodes, (const Selva_NodeId *)setOpts->$delete);
            }
            if (err < 0) {
                return err;
            }
            res += 1;
        }

        return res;
    }
}

static int update_edge(
    RedisModuleCtx *ctx,
    SelvaModify_Hierarchy *hierarchy,
    Selva_NodeId node_id,
    const RedisModuleString *field,
    struct SelvaModify_OpSet *setOpts
) {
    struct SelvaModify_HierarchyNode *node;
    const unsigned constraint_id = setOpts->edge_constraint_id;
    TO_STR(field);

    /* TODO This lookup could be optimized away. */
    node = SelvaHierarchy_FindNode(hierarchy, node_id);
    if (unlikely(!node)) {
        return SELVA_ENOENT;
    }

    if (setOpts->$value_len > 0) {
        int res = 0;

        if (setOpts->$value_len % SELVA_NODE_ID_SIZE) {
            return SELVA_EINVAL;
        }

        SVECTOR_AUTOFREE(new_ids);

        /* The comparator works for both nodes and nodeIds. */
        if (!SVector_Init(&new_ids, setOpts->$value_len / SELVA_NODE_ID_SIZE, SelvaSVectorComparator_Node)) {
            return SELVA_ENOMEM;
        }

        for (size_t i = 0; i < setOpts->$value_len; i += SELVA_NODE_ID_SIZE) {
            char *dst_node_id = setOpts->$value + i;

            SVector_Insert(&new_ids, dst_node_id);
        }

        struct EdgeField *edgeField = Edge_GetField(node, field_str, field_len);
        if (edgeField) {
            /*
             * First we remove the arcs from the old set that don't exist
             * in the new set.
             * Note that we can cast a hierarchy node to Selva_NodeId or even a
             * char as it's guaranteed that the structure starts with the id
             * that has a known length.
             */

            struct SVectorIterator it;
            SVECTOR_AUTOFREE(old_arcs);
            char *dst_id;

            if (!SVector_Clone(&old_arcs, &edgeField->arcs, NULL)) {
                return SELVA_ENOMEM;
            }

            SVector_ForeachBegin(&it, &old_arcs);
            while ((dst_id = SVector_Foreach(&it))) {
                if (!SVector_Search(&new_ids, dst_id)) {
                    Edge_Delete(field_str, field_len, node, dst_id);
                    res++; /* Count delete as a change. */
                }
            }
        }

        /*
         * Then we add the new arcs.
         */
        for (size_t i = 0; i < setOpts->$value_len; i += SELVA_NODE_ID_SIZE) {
            const char *dst_node_id = setOpts->$value + i;
            struct SelvaModify_HierarchyNode *dst_node;
            int err;

            err = SelvaHierarchy_UpsertNode(ctx, hierarchy, dst_node_id, &dst_node);
            if ((err && err != SELVA_MODIFY_HIERARCHY_EEXIST) || !dst_node) {
                /* TODO Should probably return an error */
                fprintf(stderr, "%s:%d: Upserting a node failed: %s\n",
                        __FILE__, __LINE__,
                        getSelvaErrorStr(err));
                continue;
            }

            err = Edge_Add(field_str, field_len, constraint_id, node, dst_node);
            if (!err) {
                res++;
            } else if (err != SELVA_EEXIST) {
                /* TODO Handle error */
                fprintf(stderr, "%s:%d: Adding an edge from %.*s.%.*s to %.*s failed with an error: %s\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, node_id,
                        (int)field_len, field_str,
                        (int)SELVA_NODE_ID_SIZE, dst_node_id,
                        getSelvaErrorStr(err));
                continue;
            }
        }

        return res;
    } else {
        int res = 0;

        if (setOpts->$add_len % SELVA_NODE_ID_SIZE ||
            setOpts->$delete_len % SELVA_NODE_ID_SIZE) {
            return SELVA_EINVAL;
        }

        if (setOpts->$add_len > 0) {
            for (size_t i = 0; i < setOpts->$add_len; i += SELVA_NODE_ID_SIZE) {
                struct SelvaModify_HierarchyNode *dst_node;
                int err;

                err = SelvaHierarchy_UpsertNode(ctx, hierarchy, setOpts->$add + i, &dst_node);
                if ((err && err != SELVA_MODIFY_HIERARCHY_EEXIST) || !dst_node) {
                    /* TODO Should probably return an error */
                    fprintf(stderr, "%s:%d: Upserting a node failed: %s\n",
                            __FILE__, __LINE__,
                            getSelvaErrorStr(err));
                    continue;
                }

                err = Edge_Add(field_str, field_len, constraint_id, node, dst_node);
                if (!err) {
                    res++;
                } else if (err != SELVA_EEXIST) {
                    /* TODO Handle error */
                    continue;
                }
            }
        }
        if (setOpts->$delete_len > 0) {
            for (size_t i = 0; i < setOpts->$delete_len; i += SELVA_NODE_ID_SIZE) {
                Selva_NodeId dst_node_id;
                int err;

                /*
                 * It may or may not be better for caching to have the node_id in
                 * stack.
                 */
                memcpy(dst_node_id, setOpts->$delete + i, SELVA_NODE_ID_SIZE);
                err = Edge_Delete(field_str, field_len, node, dst_node_id);
                if (!err) {
                    res++;
                }
            }
        }

        return res;
    }
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
 * @returns The number of items added; Otherwise a negative Selva error code is returned.
 */
static int add_set_values(
    RedisModuleCtx *ctx,
    SelvaModify_Hierarchy *hierarchy,
    RedisModuleKey *alias_key,
    struct SelvaObject *obj,
    RedisModuleString *id,
    const RedisModuleString *field,
    const char *value_ptr,
    size_t value_len,
    int8_t type,
    int remove_diff
) {
    const char *ptr = value_ptr;
    int res = 0;

    if (type == SELVA_MODIFY_OP_SET_TYPE_CHAR ||
        type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) {
        SVector new_set;

        /* Check that the value divides into elements properly. */
        if ((type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE && (value_len % SELVA_NODE_ID_SIZE)) ||
            (type == SELVA_MODIFY_OP_SET_TYPE_DOUBLE && (value_len % sizeof(double))) ||
            (type == SELVA_MODIFY_OP_SET_TYPE_LONG_LONG && (value_len % sizeof(long long)))) {
            return SELVA_EINVAL;
        }

        if (remove_diff) {
            size_t inital_size = (type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) ? value_len / SELVA_NODE_ID_SIZE : 1;

            SVector_Init(&new_set, inital_size, SelvaSVectorComparator_RMS);
        } else {
            /* If it's empty the destroy function will just skip over. */
            memset(&new_set, 0, sizeof(new_set));
        }

        /*
         * Add missing elements to the set.
         */
        for (size_t i = 0; i < value_len; ) {
            int err;
            RedisModuleString *ref;
            const ssize_t part_len = string2rms(ctx, type, ptr, &ref);

            if (part_len < 0) {
                res = SELVA_EINVAL;
                goto string_err;
            }

            /* Add to the node object. */
            err = SelvaObject_AddStringSet(obj, field, ref);
            if (remove_diff && (err == 0 || err == SELVA_EEXIST)) {
                SVector_InsertFast(&new_set, ref);
            }
            if (err == 0) {
                RedisModule_RetainString(ctx, ref);

                /* Add to the global aliases hash. */
                if (alias_key) {
                    Selva_NodeId node_id;
                    TO_STR(id);

                    Selva_NodeIdCpy(node_id, id_str);
                    Selva_Subscriptions_DeferAliasChangeEvents(ctx, hierarchy, ref);

                    update_alias(ctx, alias_key, id, ref);
                }

                res++;
            } else if (err != SELVA_EEXIST) {
                if (alias_key) {
                    fprintf(stderr, "%s:%d: Alias update failed\n", __FILE__, __LINE__);
                } else {
                    fprintf(stderr, "%s:%d: String set field update failed\n", __FILE__, __LINE__);
                }
                res = err;
                goto string_err;
            }

            /* +1 to skip the NUL if cstring */
            const size_t skip_off = type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE ? SELVA_NODE_ID_SIZE : (size_t)part_len + (type == SELVA_MODIFY_OP_SET_TYPE_CHAR);
            if (skip_off == 0) {
                res = SELVA_EINVAL;
                goto string_err;
            }

            ptr += skip_off;
            i += skip_off;
        }

        /*
         * Remove elements that are not in new_set.
         * This makes the set in obj.field equal to the set defined by value_str.
         */
        if (remove_diff) {
            struct SelvaSetElement *set_el;
            struct SelvaSetElement *tmp;
            struct SelvaSet *objSet = SelvaObject_GetSet(obj, field);

            assert(objSet);
            SELVA_SET_RMS_FOREACH_SAFE(set_el, objSet, tmp) {
                RedisModuleString *el = set_el->value_rms;

                if (!SVector_Search(&new_set, (void *)el)) {
                    /* el doesn't exist in new_set, therefore it should be removed. */
                    SelvaSet_DestroyElement(SelvaSet_Remove(objSet, el));

                    if (alias_key) {
                        Selva_Subscriptions_DeferAliasChangeEvents(ctx, hierarchy, el);
                        RedisModule_HashSet(alias_key, REDISMODULE_HASH_NONE, el, REDISMODULE_HASH_DELETE, NULL);
                    }

                    RedisModule_FreeString(ctx, el);
                    res++; /* This too is a change to the set! */
                }
            }
        }
string_err:
        SVector_Destroy(&new_set);
    } else if (type == SELVA_MODIFY_OP_SET_TYPE_DOUBLE ||
               type == SELVA_MODIFY_OP_SET_TYPE_LONG_LONG) {
        /*
         * Add missing elements to the set.
         */
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
            if (err == 0) {
                res++;
            } else if (err != SELVA_EEXIST) {
                fprintf(stderr, "%s:%d: Set (%s) field update failed: %s\n",
                        __FILE__, __LINE__,
                        (type == SELVA_MODIFY_OP_SET_TYPE_DOUBLE) ? "double" : "long long",
                        getSelvaErrorStr(err));
                return err;
            }

            const size_t skip_off = part_len;
            if (skip_off == 0) {
                return SELVA_EINVAL;
            }

            ptr += skip_off;
            i += skip_off;
        }

        /*
         * Remove elements that are not in new_set.
         * This makes the set in obj.field equal to the set defined by value_str.
         */
        if (remove_diff) {
            struct SelvaSetElement *set_el;
            struct SelvaSetElement *tmp;
            struct SelvaSet *objSet = SelvaObject_GetSet(obj, field);

            assert(objSet);
            if (type == SELVA_MODIFY_OP_SET_TYPE_DOUBLE && objSet->type == SELVA_SET_TYPE_DOUBLE) {
                SELVA_SET_DOUBLE_FOREACH_SAFE(set_el, objSet, tmp) {
                    int found = 0;
                    const double a = set_el->value_d;

                    /* This is probably faster than any data structure we could use. */
                    for (size_t i = 0; i < value_len; i += sizeof(double)) {
                        double b;

                        /*
                         * We use memcpy here because it's not guranteed that the
                         * array is aligned properly.
                         */
                        memcpy(&b, value_ptr + i, sizeof(double));

                        if (a == b) {
                            found = 1;
                            break;
                        }
                    }

                    if (!found) {
                        SelvaSet_DestroyElement(SelvaSet_RemoveDouble(objSet, a));
                        res++;
                    }
                }
            } else if (type == SELVA_MODIFY_OP_SET_TYPE_LONG_LONG && objSet->type == SELVA_SET_TYPE_LONGLONG) {
                SELVA_SET_LONGLONG_FOREACH_SAFE(set_el, objSet, tmp) {
                    int found = 0;
                    const long long a = set_el->value_ll;

                    /* This is probably faster than any data structure we could use. */
                    for (size_t i = 0; i < value_len; i++) {
                        long long b;

                        /*
                         * We use memcpy here because it's not guranteed that the
                         * array is aligned properly.
                         */
                        memcpy(&b, value_ptr + i, sizeof(long long));

                        if (a == b) {
                            found = 1;
                            break;
                        }
                    }

                    if (!found) {
                        SelvaSet_DestroyElement(SelvaSet_RemoveLongLong(objSet, a));
                        res++;
                    }
                }
            } else {
                abort(); /* Never reached. */
            }
        }
    } else {
        return SELVA_EINTYPE;
    }

    return res;
}

static int del_set_values(
    RedisModuleCtx *ctx,
    RedisModuleKey *alias_key,
    struct SelvaObject *obj,
    const RedisModuleString *field,
    const char *value_ptr,
    size_t value_len,
    int8_t type
) {
    const char *ptr = value_ptr;
    int res = 0;

    if (type == SELVA_MODIFY_OP_SET_TYPE_CHAR ||
        type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) {
        if (type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE && (value_len % SELVA_NODE_ID_SIZE)) {
            return SELVA_EINVAL;
        }

        for (size_t i = 0; i < value_len; ) {
            RedisModuleString *ref;
            const ssize_t part_len = string2rms(ctx, type, ptr, &ref);
            int err;

            if (part_len < 0) {
                return SELVA_EINVAL;
            }

            /* Remove from the node object. */
            err = SelvaObject_RemStringSet(obj, field, ref);
            if (!err) {
                res++;
            }

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
            if (err &&
                err != SELVA_ENOENT &&
                err != SELVA_EEXIST &&
                err != SELVA_EINVAL) {
                fprintf(stderr, "%s:%d: Double set field update failed\n", __FILE__, __LINE__);
                return err;
            }
            if (err == 0) {
                res++;
            }

            const size_t skip_off = part_len;
            ptr += skip_off;
            i += skip_off;
        }
    } else {
        return SELVA_EINTYPE;
    }

    return res;
}

/*
 * @returns The "rough" absolute number of changes made; Otherise a negative Selva error code is returned.
 */
static int update_set(
    RedisModuleCtx *ctx,
    SelvaModify_Hierarchy *hierarchy,
    struct SelvaObject *obj,
    RedisModuleString *id,
    const RedisModuleString *field,
    struct SelvaModify_OpSet *setOpts
) {
    TO_STR(field);
    RedisModuleKey *alias_key = NULL;
    int res = 0;

    if (!strcmp(field_str, SELVA_ALIASES_FIELD)) {
        alias_key = open_aliases_key(ctx);
        if (!alias_key) {
            fprintf(stderr, "%s:%d: Unable to open aliases\n", __FILE__, __LINE__);
            return SELVA_ENOENT;
        }
    }

    if (setOpts->$value_len > 0) {
        int err;

        /*
         * Set new values.
         */
        err = add_set_values(ctx, hierarchy, alias_key, obj, id, field, setOpts->$value, setOpts->$value_len, setOpts->op_set_type, 1);
        if (err < 0) {
            return err;
        } else {
            res += err;
        }
    } else {
        if (setOpts->$add_len > 0) {
            int err;

            err = add_set_values(ctx, hierarchy, alias_key, obj, id, field, setOpts->$add, setOpts->$add_len, setOpts->op_set_type, 0);
            if (err < 0) {
                return err;
            } else {
                res += err;
            }
        }

        if (setOpts->$delete_len > 0) {
            int err;

            err = del_set_values(ctx, alias_key, obj, field, setOpts->$delete, setOpts->$delete_len, setOpts->op_set_type);
            if (err < 0) {
                return err;
            }
            res += err;
        }
    }

    return res;
}

int SelvaModify_ModifySet(
    RedisModuleCtx *ctx,
    SelvaModify_Hierarchy *hierarchy,
    struct SelvaObject *obj,
    RedisModuleString *id,
    const RedisModuleString *field,
    struct SelvaModify_OpSet *setOpts
) {
    TO_STR(id, field);

    if (setOpts->op_set_type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) {
        Selva_NodeId node_id;
        int isChildren = !strcmp(field_str, "children");
        int isParents = !isChildren && !strcmp(field_str, "parents");

        Selva_NodeIdCpy(node_id, id_str);

        if (isChildren || isParents) {
            /*
             * Children and parents hierarchy is hardcoded for performance.
             */
            if (setOpts->delete_all) {
                /* If delete_all is set the other fields are ignored. */
                int err;

                if (isChildren) {
                    err = SelvaModify_DelHierarchyChildren(hierarchy, node_id);
                } else if (isParents) {
                    err = SelvaModify_DelHierarchyParents(hierarchy, node_id);
                } else {
                    err = Edge_ClearField(SelvaHierarchy_FindNode(hierarchy, node_id), field_str, field_len);
                }

                return err == SELVA_ENOENT ? 1 : err;
            } else {
                return update_hierarchy(ctx, hierarchy, node_id, field_str, setOpts);
            }
        } else {
            /*
             * Other graph fields are dynamic and implemented separately
             * from hierarchy.
             */
            return update_edge(ctx, hierarchy, node_id, field, setOpts);
        }
    } else {
        if (setOpts->delete_all) {
            int err;

            /*
             * First we need to delete the aliases of this node from the
             * ___selva_aliases hash.
             */
            if (!strcmp(field_str, SELVA_ALIASES_FIELD)) {
                RedisModuleKey *alias_key;
                struct SelvaSet *node_aliases;

                alias_key = open_aliases_key(ctx);
                if (!alias_key) {
                    fprintf(stderr, "%s:%d: Unable to open aliases\n",
                            __FILE__, __LINE__);
                    return SELVA_ENOENT;
                }

                node_aliases = SelvaObject_GetSet(obj, field);
                if (node_aliases) {
                    selva_set_defer_alias_change_events(ctx, hierarchy, node_aliases);
                    (void)delete_aliases(alias_key, node_aliases);
                    /* TODO It would be nice to return the actual number of deletions. */
                }
            }

            err = SelvaObject_DelKey(obj, field);
            if (err == 0) {
                err = 1;
            }

            return err == SELVA_ENOENT ? 0 : err;
        } else {
            /*
             * Other set ops use C-strings and operate on the node SelvaObject.
             */
            return update_set(ctx, hierarchy, obj, id, field, setOpts);
        }
    }
}

void SelvaModify_ModifyIncrement(
    struct SelvaObject *obj,
    const RedisModuleString *field,
    enum SelvaObjectType old_type,
    const struct SelvaModify_OpIncrement *incrementOpts
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
    RedisModuleCtx *ctx __unused,
    struct SelvaObject *obj,
    const RedisModuleString *field,
    enum SelvaObjectType old_type,
    const struct SelvaModify_OpIncrementDouble *incrementOpts
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
    const RedisModuleString *field
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
        err = SelvaObject_DelKey(obj, field);
    }

    return err;
}
