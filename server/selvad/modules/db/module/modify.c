/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "jemalloc.h"
#include "libdeflate.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_replication.h"
#include "selva_server.h"
#include "util/bitmap.h"
#include "util/cstrings.h"
#include "util/finalizer.h"
#include "util/selva_proto_builder.h"
#include "util/selva_string.h"
#include "util/svector.h"
#include "util/timestamp.h"
#include "arg_parser.h"
#include "async_task.h"
#include "comparator.h"
#include "db_config.h"
#include "hierarchy.h"
#include "selva_db.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "selva_trace.h"
#include "subscriptions.h"
#include "typestr.h"
#include "modify.h"

#define FLAG_NO_ROOT    0x01 /*!< Don't set root as a parent. */
#define FLAG_NO_MERGE   0x02 /*!< Clear any existing fields. */
#define FLAG_CREATE     0x04 /*!< Only create a new node or fail. */
#define FLAG_UPDATE     0x08 /*!< Only update an existing node. */

#define FISSET_NO_ROOT(m) (((m) & FLAG_NO_ROOT) == FLAG_NO_ROOT)
#define FISSET_NO_MERGE(m) (((m) & FLAG_NO_MERGE) == FLAG_NO_MERGE)
#define FISSET_CREATE(m) (((m) & FLAG_CREATE) == FLAG_CREATE)
#define FISSET_UPDATE(m) (((m) & FLAG_UPDATE) == FLAG_UPDATE)

#define REPLY_WITH_ARG_TYPE_ERROR(v) \
    selva_send_errorf(resp, SELVA_EINTYPE, "Expected: %s", typeof_str(v))

/**
 * Struct type for replicating the automatic timestamps.
 */
struct replicate_ts {
    int8_t created;
    int8_t updated;
    long long created_at;
    long long updated_at;
};

SELVA_TRACE_HANDLE(cmd_modify);

static ssize_t string2selva_string(struct finalizer *fin, int8_t type, const char *s, struct selva_string **out) {
    size_t len;

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

    *out = selva_string_create(s, len, 0);
    if (fin) {
        finalizer_add(fin, *out, selva_string_free);
    }

    return len;
}

static int update_hierarchy(
    SelvaHierarchy *hierarchy,
    const Selva_NodeId node_id,
    const char *field_str,
    const struct SelvaModify_OpSet *setOpts
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
            SELVA_LOG(SELVA_LOGL_DBG, "Set parents of %.*s nr_nodes: %zu ",
                      (int)SELVA_NODE_ID_SIZE, node_id,
                      nr_nodes);
            for (size_t i = 0; i < nr_nodes; i++) {
                SELVA_LOG(SELVA_LOGL_DBG, "%.*s, ", (int)SELVA_NODE_ID_SIZE, ((const Selva_NodeId *)setOpts->$value)[i]);

            }
#endif

            err = SelvaModify_SetHierarchyParents(hierarchy, node_id,
                    nr_nodes, (const Selva_NodeId *)setOpts->$value);
        } else { /* children */
#if 0
            SELVA_LOG(SELVA_LOGL_DBG, "Set children of %.*s nr_nodes: %zu",
                      (int)SELVA_NODE_ID_SIZE, node_id,
                      nr_nodes);
            for (size_t i = 0; i < nr_nodes; i++) {
                SELVA_LOG(SELVA_LOGL_DBG, "%.*s, ", (int)SELVA_NODE_ID_SIZE, ((const Selva_NodeId *)setOpts->$value)[i]);

            }
#endif

            err = SelvaModify_SetHierarchyChildren(hierarchy, node_id,
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
                SELVA_LOG(SELVA_LOGL_DBG, "Add to parents of %.*s nr_nodes: %zu",
                          (int)SELVA_NODE_ID_SIZE, node_id,
                          nr_nodes);
#endif

                err = SelvaModify_AddHierarchy(hierarchy, node_id,
                        nr_nodes, (const Selva_NodeId *)setOpts->$add,
                        0, NULL);
            } else { /* children */
#if 0
                SELVA_LOG(SELVA_LOGL_DBG, "Add to children of %.*s nr_nodes: %zu",
                          (int)SELVA_NODE_ID_SIZE, node_id,
                          nr_nodes);
#endif

                err = SelvaModify_AddHierarchy(hierarchy, node_id,
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
                err = SelvaModify_DelHierarchy(hierarchy, node_id,
                        nr_nodes, (const Selva_NodeId *)setOpts->$delete,
                        0, NULL);
            } else { /* children */
                err = SelvaModify_DelHierarchy(hierarchy, node_id,
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
    SelvaHierarchy *hierarchy,
    struct SelvaHierarchyNode *node,
    const struct selva_string *field,
    const struct SelvaModify_OpSet *setOpts
) {
    const unsigned constraint_id = setOpts->edge_constraint_id;
    TO_STR(field);

    if (setOpts->$value_len > 0) {
        int res = 0;

        if (setOpts->$value_len % SELVA_NODE_ID_SIZE) {
            return SELVA_EINVAL;
        }

        SVECTOR_AUTOFREE(new_ids);

        /* The comparator works for both nodes and nodeIds. */
        SVector_Init(&new_ids, setOpts->$value_len / SELVA_NODE_ID_SIZE, SelvaSVectorComparator_Node);

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
                    Edge_Delete(hierarchy, edgeField, node, dst_id);
                    res++; /* Count delete as a change. */
                }
            }
        }

        /*
         * Then we add the new arcs.
         */
        for (size_t i = 0; i < setOpts->$value_len; i += SELVA_NODE_ID_SIZE) {
            const char *dst_node_id = setOpts->$value + i;
            struct SelvaHierarchyNode *dst_node;
            int err;

            err = SelvaHierarchy_UpsertNode(hierarchy, dst_node_id, &dst_node);
            if ((err && err != SELVA_HIERARCHY_EEXIST) || !dst_node) {
                SELVA_LOG(SELVA_LOGL_ERR, "Upserting a node failed: %s",
                          selva_strerror(err));
                /*
                 * We could also ignore the error and try to insert the rest but
                 * perhaps it can be considered a fatal error if one of the
                 * nodes cannot be referenced/created.
                 */
                return err;
            }

            err = Edge_Add(hierarchy, constraint_id, field_str, field_len, node, dst_node);
            if (!err) {
                res++;
            } else if (err != SELVA_EEXIST) {
                /*
                 * This will most likely happen in real production only when
                 * the constraints don't match.
                 */
#if 0
                SELVA_LOG(SELVA_LOGL_DBG, "Adding an edge from %.*s.%.*s to %.*s failed with an error: %s",
                          (int)SELVA_NODE_ID_SIZE, node_id,
                          (int)field_len, field_str,
                          (int)SELVA_NODE_ID_SIZE, dst_node_id,
                          selva_strerror(err));
#endif
                return err;
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
                struct SelvaHierarchyNode *dst_node;
                int err;

                err = SelvaHierarchy_UpsertNode(hierarchy, setOpts->$add + i, &dst_node);
                if ((err && err != SELVA_HIERARCHY_EEXIST) || !dst_node) {
                    /* See similar case with $value */
                    SELVA_LOG(SELVA_LOGL_ERR, "Upserting a node failed: %s\n",
                              selva_strerror(err));
                    return err;
                }

                err = Edge_Add(hierarchy, constraint_id, field_str, field_len, node, dst_node);
                if (!err) {
                    res++;
                } else if (err != SELVA_EEXIST) {
                    /*
                     * This will most likely happen in real production only when
                     * the constraints don't match.
                     */
#if 0
                    SELVA_LOG(SELVA_LOGL_DBG, "Adding an edge from %.*s.%.*s to %.*s failed with an error: %s",
                              (int)SELVA_NODE_ID_SIZE, node_id,
                              (int)field_len, field_str,
                              (int)SELVA_NODE_ID_SIZE, dst_node_id,
                              selva_strerror(err));
#endif
                    return err;
                }
            }
        }
        if (setOpts->$delete_len > 0) {
            struct EdgeField *edgeField = Edge_GetField(node, field_str, field_len);
            if (edgeField) {
                for (size_t i = 0; i < setOpts->$delete_len; i += SELVA_NODE_ID_SIZE) {
                    Selva_NodeId dst_node_id;
                    int err;

                    /*
                     * It may or may not be better for caching to have the node_id in
                     * stack.
                     */
                    memcpy(dst_node_id, setOpts->$delete + i, SELVA_NODE_ID_SIZE);
                    err = Edge_Delete(hierarchy, edgeField, node, dst_node_id);
                    if (!err) {
                        res++;
                    }
                }
            }
        }

        return res;
    }
}

static void selva_set_defer_alias_change_events(
        SelvaHierarchy *hierarchy,
        struct SelvaSet *aliases) {
    struct SelvaSetElement *el;

    SELVA_SET_STRING_FOREACH(el, aliases) {
        struct selva_string *alias_name = el->value_string;

        SelvaSubscriptions_DeferAliasChangeEvents(hierarchy, alias_name);
    }
}

/**
 * Add all values from value_ptr to the set in obj.field.
 * @returns The number of items added; Otherwise a negative Selva error code is returned.
 */
static int add_set_values(
    SelvaHierarchy *hierarchy,
    struct SelvaObject *obj,
    const Selva_NodeId node_id,
    const struct selva_string *field,
    const char *value_ptr,
    size_t value_len,
    int8_t type,
    int remove_diff
) {
    TO_STR(field);
    const int is_aliases = !strcmp(field_str, SELVA_ALIASES_FIELD);
    const char *ptr = value_ptr;
    int res = 0;

    if (type == SELVA_MODIFY_OP_SET_TYPE_CHAR ||
        type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) {
        SVector new_set;
        __auto_finalizer struct finalizer fin;

        /* Check that the value divides into elements properly. */
        if ((type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE && (value_len % SELVA_NODE_ID_SIZE)) ||
            (type == SELVA_MODIFY_OP_SET_TYPE_DOUBLE && (value_len % sizeof(double))) ||
            (type == SELVA_MODIFY_OP_SET_TYPE_LONG_LONG && (value_len % sizeof(long long)))) {
            return SELVA_EINVAL;
        }

        if (remove_diff) {
            size_t inital_size = (type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) ? value_len / SELVA_NODE_ID_SIZE : 1;

            SVector_Init(&new_set, inital_size, SelvaSVectorComparator_String);
        } else {
            /* If it's empty the destroy function will just skip over. */
            memset(&new_set, 0, sizeof(new_set));
        }
        finalizer_init(&fin);

        /*
         * Add missing elements to the set.
         */
        for (size_t i = 0; i < value_len; ) {
            int err;
            struct selva_string *ref;
            const ssize_t part_len = string2selva_string(&fin, type, ptr, &ref);

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
                finalizer_del(&fin, ref);

                /* Add to the global aliases hash. */
                if (is_aliases) {
                    SelvaSubscriptions_DeferAliasChangeEvents(hierarchy, ref);

                    update_alias(hierarchy, node_id, ref);
                }

                res++;
            } else if (err != SELVA_EEXIST) {
                if (is_aliases) {
                    SELVA_LOG(SELVA_LOGL_ERR, "Alias update failed");
                } else {
                    SELVA_LOG(SELVA_LOGL_ERR, "String set field update failed");
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
            SELVA_SET_STRING_FOREACH_SAFE(set_el, objSet, tmp) {
                struct selva_string *el = set_el->value_string;

                if (!SVector_Search(&new_set, (void *)el)) {
                    /* el doesn't exist in new_set, therefore it should be removed. */
                    SelvaSet_DestroyElement(SelvaSet_Remove(objSet, el));

                    if (is_aliases) {
                        SelvaSubscriptions_DeferAliasChangeEvents(hierarchy, el);
                        delete_alias(hierarchy, el);
                    }

                    selva_string_free(el);
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
                SELVA_LOG(SELVA_LOGL_ERR, "Set (%s) field update failed: %s\n",
                          (type == SELVA_MODIFY_OP_SET_TYPE_DOUBLE) ? "double" : "long long",
                          selva_strerror(err));
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
        struct SelvaHierarchy *hierarchy,
        struct SelvaObject *obj,
        const struct selva_string *field,
        const char *value_ptr,
        size_t value_len,
        int8_t type
) {
    TO_STR(field);
    const int is_aliases = !strcmp(field_str, SELVA_ALIASES_FIELD);
    const char *ptr = value_ptr;
    int res = 0;

    if (type == SELVA_MODIFY_OP_SET_TYPE_CHAR ||
        type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) {
        if (type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE && (value_len % SELVA_NODE_ID_SIZE)) {
            return SELVA_EINVAL;
        }

        for (size_t i = 0; i < value_len; ) {
            struct selva_string *ref;
            const ssize_t part_len = string2selva_string(NULL, type, ptr, &ref);
            int err;

            if (part_len < 0) {
                return SELVA_EINVAL;
            }

            /*
             * Remove from the node object.
             */
            err = SelvaObject_RemStringSet(obj, field, ref);
            if (!err) {
                res++;
            }
            selva_string_free(ref);

            /*
             * Remove from the global aliases hash.
             */
            if (is_aliases) {
                delete_alias(hierarchy, ref);
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
                SELVA_LOG(SELVA_LOGL_ERR, "Double set field update failed");
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
    SelvaHierarchy *hierarchy,
    struct SelvaObject *obj,
    const Selva_NodeId node_id,
    const struct selva_string *field,
    const struct SelvaModify_OpSet *setOpts
) {
    int res = 0;

    if (setOpts->$value_len > 0) {
        int err;

        /*
         * Set new values.
         */
        err = add_set_values(hierarchy, obj, node_id, field, setOpts->$value, setOpts->$value_len, setOpts->op_set_type, 1);
        if (err < 0) {
            return err;
        } else {
            res += err;
        }
    } else {
        if (setOpts->$add_len > 0) {
            int err;

            err = add_set_values(hierarchy, obj, node_id, field, setOpts->$add, setOpts->$add_len, setOpts->op_set_type, 0);
            if (err < 0) {
                return err;
            } else {
                res += err;
            }
        }

        if (setOpts->$delete_len > 0) {
            int err;

            err = del_set_values(hierarchy, obj, field, setOpts->$delete, setOpts->$delete_len, setOpts->op_set_type);
            if (err < 0) {
                return err;
            }
            res += err;
        }
    }

    return res;
}

int SelvaModify_ModifySet(
    SelvaHierarchy *hierarchy,
    const Selva_NodeId node_id,
    struct SelvaHierarchyNode *node,
    struct SelvaObject *obj,
    const struct selva_string *field,
    struct SelvaModify_OpSet *setOpts
) {
    TO_STR(field);

    if (setOpts->op_set_type == SELVA_MODIFY_OP_SET_TYPE_REFERENCE) {
        int isChildren = !strcmp(field_str, "children");
        int isParents = !isChildren && !strcmp(field_str, "parents");

        if (setOpts->delete_all) {
            /* If delete_all is set the other fields are ignored. */
            int err;

            if (isChildren) {
                err = SelvaHierarchy_DelChildren(hierarchy, node);
            } else if (isParents) {
                err = SelvaHierarchy_DelParents(hierarchy, node);
            } else {
                err = Edge_ClearField(hierarchy, node, field_str, field_len);
            }

            if (err >= 0) {
                return err;
            } else if (err == SELVA_ENOENT || err == SELVA_HIERARCHY_ENOENT) {
                return 0;
            } else {
                return err;
            }
        } else if (isChildren || isParents) {
            return update_hierarchy(hierarchy, node_id, field_str, setOpts);
        } else {
            /*
             * Other graph fields are dynamic and implemented separately
             * from hierarchy.
             */
            return update_edge(hierarchy, node, field, setOpts);
        }
    } else {
        if (setOpts->delete_all) {
            int err;

            /*
             * First we need to delete the aliases of this node from the
             * ___selva_aliases hash.
             */
            if (!strcmp(field_str, SELVA_ALIASES_FIELD)) {
                struct SelvaSet *node_aliases;

                node_aliases = SelvaObject_GetSet(obj, field);
                if (node_aliases) {
                    selva_set_defer_alias_change_events(hierarchy, node_aliases);
                    (void)delete_aliases(hierarchy, node_aliases);
                }
            }

            err = SelvaObject_DelKey(obj, field);
            if (err == 0) {
                /* TODO It would be nice to return the actual number of deletions. */
                err = 1;
            }

            return err == SELVA_ENOENT ? 0 : err;
        } else {
            /*
             * Other set ops use C-strings and operate on the node SelvaObject.
             */
            return update_set(hierarchy, obj, node_id, field, setOpts);
        }
    }
}

int SelvaModify_ModifyDel(
    SelvaHierarchy *hierarchy,
    struct SelvaHierarchyNode *node,
    struct SelvaObject *obj,
    const struct selva_string *field
) {
    TO_STR(field);
    int err;

    if (!strcmp(field_str, "children")) {
        err = SelvaHierarchy_DelChildren(hierarchy, node);
    } else if (!strcmp(field_str, "parents")) {
        err = SelvaHierarchy_DelParents(hierarchy, node);
    } else { /* It's either an edge field or an object field. */
        err = Edge_DeleteField(hierarchy, node, field_str, field_len);
        if (err == SELVA_ENOENT) {
            /* Finally let's try if it's an object field. */
            err = SelvaObject_DelKeyStr(obj, field_str, field_len);
        }
    }

    return err > 0 ? 0 : err;
}

/*
 * Tokenize nul-terminated strings from a string with the size of size.
 */
static const char *sztok(const char *s, size_t size, size_t * restrict i) {
    const char *r = NULL;

    if (size == 0) {
        return NULL;
    }

    if (*i < size - 1) {
        r = s + *i;
        *i = *i + strnlen(r, size) + 1;
    }

    return r;
}

static int parse_flags(const struct selva_string *arg) {
    TO_STR(arg);
    int flags = 0;

    for (size_t i = 0; i < arg_len; i++) {
        flags |= arg_str[i] == 'N' ? FLAG_NO_ROOT : 0;
        flags |= arg_str[i] == 'M' ? FLAG_NO_MERGE : 0;
        flags |= arg_str[i] == 'C' ? FLAG_CREATE : 0;
        flags |= arg_str[i] == 'U' ? FLAG_UPDATE : 0;
    }

    return flags;
}

/**
 * Parse $alias query from the command args if one exists.
 * @param out a vector for the query.
 *            The SVector must be initialized before calling this function.
 */
static void parse_alias_query(struct selva_string **argv, int argc, SVector *out) {
    for (int i = 0; i < argc; i += 3) {
        const struct selva_string *type = argv[i];
        const struct selva_string *field = argv[i + 1];
        const struct selva_string *value = argv[i + 2];

        TO_STR(type, field, value);
        char type_code = type_str[0];

        if (type_code == SELVA_MODIFY_ARG_STRING_ARRAY && !strcmp(field_str, "$alias")) {
            const char *s;
            size_t j = 0;
            while ((s = sztok(value_str, value_len, &j))) {
                SVector_Insert(out, (void *)s);
            }
        }
    }
}

static int in_mem_range(const void *p, const void *start, size_t size) {
    return (ptrdiff_t)p >= (ptrdiff_t)start && (ptrdiff_t)p < (ptrdiff_t)start + (ptrdiff_t)size;
}

struct SelvaModify_OpSet *SelvaModify_OpSet_align(struct finalizer *fin, const struct selva_string *data) {
    TO_STR(data);
    struct SelvaModify_OpSet *op;

    _Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "Only little endian host is supported");

    if (!data || data_len == 0 || data_len < sizeof(struct SelvaModify_OpSet)) {
        return NULL;
    }

    op = selva_malloc(data_len);
    finalizer_add(fin, op, selva_free);

    memcpy(op, data_str, data_len);
    op->$add    = op->$add    ? ((char *)op + (ptrdiff_t)op->$add)    : NULL;
    op->$delete = op->$delete ? ((char *)op + (ptrdiff_t)op->$delete) : NULL;
    op->$value  = op->$value  ? ((char *)op + (ptrdiff_t)op->$value)  : NULL;

    if (!(((!op->$add    && op->$add_len == 0)    || (in_mem_range(op->$add,    op, data_len) && in_mem_range(op->$add    + op->$add_len    - 1,  op, data_len))) &&
          ((!op->$delete && op->$delete_len == 0) || (in_mem_range(op->$delete, op, data_len) && in_mem_range(op->$delete + op->$delete_len - 1,  op, data_len))) &&
          ((!op->$value  && op->$value_len == 0)  || (in_mem_range(op->$value,  op, data_len) && in_mem_range(op->$value  + op->$value_len  - 1,  op, data_len)))
       )) {
        return NULL;
    }

    return op;
}

static struct SelvaModify_OpEdgeMeta *SelvaModify_OpEdgeMeta_align(struct finalizer *fin, const struct selva_string *data) {
    TO_STR(data);
    struct SelvaModify_OpEdgeMeta *op;

    _Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "Only little endian host is supported");

    if (!data || data_len == 0 || data_len < sizeof(struct SelvaModify_OpEdgeMeta)) {
        return NULL;
    }

    op = selva_malloc(data_len);
    finalizer_add(fin, op, selva_free);

    memcpy(op, data_str, data_len);
    if (!op->meta_field_name_str || !op->meta_field_value_str) {
        return NULL;
    }

    op->meta_field_name_str = ((char *)op + (ptrdiff_t)op->meta_field_name_str);
    op->meta_field_value_str = ((char *)op + (ptrdiff_t)op->meta_field_value_str);

    if (!((in_mem_range(op->meta_field_name_str,  op, data_len) && in_mem_range(op->meta_field_name_str  + op->meta_field_name_len  - 1, op, data_len)) &&
          (in_mem_range(op->meta_field_value_str, op, data_len) && in_mem_range(op->meta_field_value_str + op->meta_field_value_len - 1, op, data_len)))) {
        return NULL;
    }

    return op;
}

/**
 * Get the replicate_ts struct.
 */
void get_replicate_ts(struct replicate_ts *rs, struct SelvaHierarchyNode *node, bool created, bool updated) {
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    rs->created = created ? 1 : 0;
    rs->updated = updated ? 1 : 0;

    if (created) {
        (void)SelvaObject_GetLongLongStr(obj, SELVA_CREATED_AT_FIELD, sizeof(SELVA_CREATED_AT_FIELD) - 1, &rs->created_at);
    }
    if (updated) {
        (void)SelvaObject_GetLongLongStr(obj, SELVA_UPDATED_AT_FIELD, sizeof(SELVA_UPDATED_AT_FIELD) - 1, &rs->updated_at);
    }
}

enum selva_op_repl_state SelvaModify_ModifyMetadata(
        struct selva_server_response_out *resp,
        struct SelvaObject *obj,
        const struct selva_string *field,
        const struct selva_string *value) {
    TO_STR(value);
    SelvaObjectMeta_t new_user_meta;
    SelvaObjectMeta_t old_user_meta;
    int err;

    if (value_len < sizeof(SelvaObjectMeta_t)) {
        REPLY_WITH_ARG_TYPE_ERROR(new_user_meta);
        return SELVA_OP_REPL_STATE_UNCHANGED;
    }

    memcpy(&new_user_meta, value_str, sizeof(SelvaObjectMeta_t));
    err = SelvaObject_SetUserMeta(obj, field, new_user_meta, &old_user_meta);
    if (err) {
        selva_send_errorf(resp, err, "Failed to set key metadata (%s)",
                          selva_string_to_str(field, NULL));
        return SELVA_OP_REPL_STATE_UNCHANGED;
    }

    if (new_user_meta != old_user_meta) {
        return SELVA_OP_REPL_STATE_UPDATED;
    }

    return SELVA_OP_REPL_STATE_REPLICATE;
}

static enum selva_op_repl_state modify_array_op(
        struct finalizer *fin,
        struct selva_server_response_out *resp,
        struct SelvaHierarchyNode *node,
        int *active_insert_idx,
        int has_push,
        char type_code,
        struct selva_string *field,
        struct selva_string *value) {
    TO_STR(field, value);
    ssize_t new_len;
    ssize_t idx;

    new_len = get_array_field_index(field_str, field_len, &idx);
    if (new_len < 0) {
        selva_send_errorf(resp, SELVA_EINVAL, "Invalid array index");
        return SELVA_OP_REPL_STATE_UNCHANGED;
    }

    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);

    if (idx == -1) {
        const int ary_len = (int)SelvaObject_GetArrayLenStr(obj, field_str, new_len);

        idx = ary_len - 1 + has_push;
        if (idx < 0) {
            selva_send_errorf(resp, SELVA_EINVAL, "Invalid array index %d", idx);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    }

    if (type_code == SELVA_MODIFY_ARG_STRING) {
        int err;

        if (*active_insert_idx == idx) {
            err = SelvaObject_InsertArrayIndexStr(obj, field_str, new_len, SELVA_OBJECT_STRING, idx, value);
            *active_insert_idx = -1;
        } else {
            err = SelvaObject_AssignArrayIndexStr(obj, field_str, new_len, SELVA_OBJECT_STRING, idx, value);
        }

        if (err) {
            selva_send_errorf(resp, err, "Failed to set a string value");
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        finalizer_del(fin, value);
    } else if (type_code == SELVA_MODIFY_ARG_DOUBLE) {
        int err;
        union {
            char s[sizeof(double)];
            double d;
            void *p;
        } v = {
            .d = 0.0,
        };

        if (value_len != sizeof(v.d)) {
            REPLY_WITH_ARG_TYPE_ERROR(v.d);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        memcpy(v.s, value_str, sizeof(v.d));

        if (*active_insert_idx == idx) {
            err = SelvaObject_InsertArrayIndexStr(obj, field_str, new_len, SELVA_OBJECT_DOUBLE, idx, v.p);
            *active_insert_idx = -1;
        } else {
            err = SelvaObject_AssignArrayIndexStr(obj, field_str, new_len, SELVA_OBJECT_DOUBLE, idx, v.p);
        }

        if (err) {
            selva_send_errorf(resp, err, "Failed to set a double value");
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_LONGLONG) {
        int err;
        union {
            char s[sizeof(double)];
            long long ll;
            void *p;
        } v = {
            .ll = 0,
        };

        if (value_len != sizeof(v.ll)) {
            REPLY_WITH_ARG_TYPE_ERROR(v.ll);
            return 0;
        }

        memcpy(v.s, value_str, sizeof(v.ll));

        if (*active_insert_idx == idx) {
            err = SelvaObject_InsertArrayIndexStr(obj, field_str, new_len, SELVA_OBJECT_LONGLONG, idx, v.p);
            *active_insert_idx = -1;
        } else {
            err = SelvaObject_AssignArrayIndexStr(obj, field_str, new_len, SELVA_OBJECT_LONGLONG, idx, v.p);
        }

        if (err) {
            selva_send_errorf(resp, err, "Failed to set a long value");
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_OBJ_META) {
        return SelvaModify_ModifyMetadata(resp, obj, field, value);
    } else {
        selva_send_errorf(resp, SELVA_EINTYPE, "ERR Invalid operation type with array syntax: \"%c\"", type_code);
        return SELVA_OP_REPL_STATE_UNCHANGED;
    }

    return SELVA_OP_REPL_STATE_UPDATED;
}

static enum selva_op_repl_state modify_op(
        struct finalizer *fin,
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId nodeId,
        struct SelvaHierarchyNode *node,
        char type_code,
        struct selva_string *field,
        struct selva_string *value) {
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    TO_STR(field, value);

    if (type_code == SELVA_MODIFY_ARG_OP_INCREMENT) {
        const struct SelvaModify_OpIncrement *incrementOpts = (const struct SelvaModify_OpIncrement *)value_str;
        int err;

        err = SelvaObject_IncrementLongLong(obj, field, incrementOpts->$default, incrementOpts->$increment, NULL);
        if (err) {
            selva_send_error(resp, err, NULL, 0);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE) {
        const struct SelvaModify_OpIncrementDouble *incrementOpts = (const struct SelvaModify_OpIncrementDouble*)value_str;
        int err;

        err = SelvaObject_IncrementDouble(obj, field, incrementOpts->$default, incrementOpts->$increment, NULL);
        if (err) {
            selva_send_error(resp, err, NULL, 0);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_SET) {
        struct SelvaModify_OpSet *setOpts;
        int err;

        setOpts = SelvaModify_OpSet_align(fin, value);
        if (!setOpts) {
            selva_send_errorf(resp, SELVA_EINVAL, "Invalid OpSet");
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        err = SelvaModify_ModifySet(hierarchy, nodeId, node, obj, field, setOpts);
        if (err == 0) {
            selva_send_str(resp, "OK", 2);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        } else if (err < 0) {
            selva_send_error(resp, err, NULL, 0);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_DEL) {
        int err;

        err = SelvaModify_ModifyDel(hierarchy, node, obj, field);
        if (err == SELVA_ENOENT) {
            /* No need to replicate. */
            selva_send_str(resp, "OK", 2);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        } else if (err) {
            selva_send_errorf(resp, err, "Failed to delete the field: \"%.*s\"",
                              (int)field_len, field_str);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_STRING ||
               type_code == SELVA_MODIFY_ARG_STRING) {
        const enum SelvaObjectType old_type = SelvaObject_GetTypeStr(obj, field_str, field_len);
        struct selva_string *old_value;
        int err;

        if (type_code == SELVA_MODIFY_ARG_DEFAULT_STRING && old_type != SELVA_OBJECT_NULL) {
            selva_send_str(resp, "OK", 2);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        if (old_type == SELVA_OBJECT_STRING && !SelvaObject_GetString(obj, field, &old_value)) {
            TO_STR(old_value);

            if (old_value_len == value_len && !memcmp(old_value_str, value_str, value_len)) {
                if (!strcmp(field_str, "type")) {
                    /*
                     * Always send "UPDATED" for the "type" field because the
                     * client will/should only send a it for a new node but
                     * typically the field is already set by using the type map.
                     */
                    selva_send_str(resp, "UPDATED", 7);
                } else {
                    selva_send_str(resp, "OK", 2);
                }
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        }

        if (SELVA_IS_TYPE_FIELD(field_str, field_len)) {
            struct selva_string *shared;

            shared = selva_string_create(value_str, value_len, SELVA_STRING_INTERN);
            err = SelvaObject_SetString(obj, field, shared);
            if (err) {
                selva_send_errorf(resp, err, "Failed to set a shared string value");
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        } else {
            err = SelvaObject_SetString(obj, field, value);
            if (err) {
                selva_send_errorf(resp, err, "Failed to set a string value");
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
            finalizer_del(fin, value);
        }
    } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_LONGLONG ||
               type_code == SELVA_MODIFY_ARG_LONGLONG) {
        long long ll;
        int err;

        if (value_len != sizeof(ll)) {
            REPLY_WITH_ARG_TYPE_ERROR(ll);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        memcpy(&ll, value_str, sizeof(ll));

        if (type_code == SELVA_MODIFY_ARG_DEFAULT_LONGLONG) {
            err = SelvaObject_SetLongLongDefault(obj, field, ll);
        } else {
            err = SelvaObject_UpdateLongLong(obj, field, ll);
        }
        if (err == SELVA_EEXIST) { /* Default handling. */
            selva_send_str(resp, "OK", 2);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        } else if (err) {
            selva_send_error(resp, err, NULL, 0);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_DEFAULT_DOUBLE ||
               type_code == SELVA_MODIFY_ARG_DOUBLE) {
        double d;
        int err;

        if (value_len != sizeof(d)) {
            REPLY_WITH_ARG_TYPE_ERROR(d);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        memcpy(&d, value_str, sizeof(d));

        if (type_code == SELVA_MODIFY_ARG_DEFAULT_DOUBLE) {
            err = SelvaObject_SetDoubleDefault(obj, field, d);
        } else {
            err = SelvaObject_UpdateDouble(obj, field, d);
        }
        if (err == SELVA_EEXIST) { /* Default handling. */
            selva_send_str(resp, "OK", 2);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        } else if (err) {
            selva_send_error(resp, err, NULL, 0);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (type_code == SELVA_MODIFY_ARG_OP_OBJ_META) {
        return SelvaModify_ModifyMetadata(resp, obj, field, value);
    } else if (type_code == SELVA_MODIFY_ARG_OP_ARRAY_REMOVE) {
        uint32_t v;
        int err;

        if (value_len != sizeof(uint32_t)) {
            REPLY_WITH_ARG_TYPE_ERROR(v);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        memcpy(&v, value_str, sizeof(uint32_t));

        err = SelvaObject_RemoveArrayIndexStr(obj, field_str, field_len, v);
        if (err) {
            selva_send_errorf(resp, err, "Failed to remove array index: %.*s[%d]",
                              (int)field_len, field_str,
                              (int)v);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else {
        selva_send_errorf(resp, SELVA_EINTYPE, "ERR Invalid type: \"%c\"", type_code);
        return SELVA_OP_REPL_STATE_UNCHANGED;
    }

    return SELVA_OP_REPL_STATE_UPDATED;
}

static enum selva_op_repl_state modify_edge_meta_op(
        struct finalizer *fin,
        struct selva_server_response_out *resp,
        struct SelvaHierarchyNode *node,
        struct selva_string *field,
        struct selva_string *raw_value) {
    TO_STR(field);
    struct EdgeField *edge_field;
    struct SelvaObject *edge_metadata;
    const struct SelvaModify_OpEdgeMeta *op;
    enum SelvaModify_OpEdgetMetaCode op_code;
    int err;

    edge_field = Edge_GetField(node, field_str, field_len);
    if (!edge_field) {
        selva_send_errorf(resp, SELVA_ENOENT, "Edge field not found");
        return SELVA_OP_REPL_STATE_UNCHANGED;
    }

    op = SelvaModify_OpEdgeMeta_align(fin, raw_value);
    if (!op) {
        selva_send_error(resp, SELVA_EINVAL, NULL, 0);
        return SELVA_OP_REPL_STATE_UNCHANGED;
    }

    if (op->delete_all) {
        Edge_DeleteFieldMetadata(edge_field);
        return SELVA_OP_REPL_STATE_UPDATED;
    }

    err = Edge_GetFieldEdgeMetadata(edge_field, op->dst_node_id, 1, &edge_metadata);
    if (err) {
        selva_send_errorf(resp, err, "Failed to get the metadata object");
        return SELVA_OP_REPL_STATE_UNCHANGED;
    }

    op_code = op->op_code;
    if (op_code == SELVA_MODIFY_OP_EDGE_META_DEFAULT_STRING ||
        op_code == SELVA_MODIFY_OP_EDGE_META_STRING) {
        const enum SelvaObjectType old_type = SelvaObject_GetTypeStr(edge_metadata, op->meta_field_name_str, op->meta_field_name_len);
        struct selva_string *old_value;
        struct selva_string *meta_field_value;

        if (op_code == SELVA_MODIFY_OP_EDGE_META_DEFAULT_STRING && old_type != SELVA_OBJECT_NULL) {
            selva_send_str(resp, "OK", 2);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        if (old_type == SELVA_OBJECT_STRING && !SelvaObject_GetStringStr(edge_metadata, op->meta_field_name_str, op->meta_field_name_len, &old_value)) {
            TO_STR(old_value);

            if (old_value_len == op->meta_field_value_len && !memcmp(old_value_str, op->meta_field_value_str, op->meta_field_value_len)) {
                selva_send_str(resp, "OK", 2);
                return SELVA_OP_REPL_STATE_UNCHANGED;
            }
        }

        meta_field_value = selva_string_create(op->meta_field_value_str, op->meta_field_value_len, 0);
        err = SelvaObject_SetStringStr(edge_metadata, op->meta_field_name_str, op->meta_field_name_len, meta_field_value);
        if (err) {
            selva_string_free(meta_field_value);
            selva_send_errorf(resp, err, "Failed to set a string value");
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (op_code == SELVA_MODIFY_OP_EDGE_META_DEFAULT_LONGLONG ||
               op_code == SELVA_MODIFY_OP_EDGE_META_LONGLONG) {
        long long ll;

        if (op->meta_field_value_len != sizeof(ll)) {
            REPLY_WITH_ARG_TYPE_ERROR(ll);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        memcpy(&ll, op->meta_field_value_str, sizeof(ll));

        if (op_code == SELVA_MODIFY_OP_EDGE_META_DEFAULT_LONGLONG) {
            err = SelvaObject_SetLongLongDefaultStr(edge_metadata, op->meta_field_name_str, op->meta_field_name_len, ll);
        } else {
            err = SelvaObject_UpdateLongLongStr(edge_metadata, op->meta_field_name_str, op->meta_field_name_len, ll);
        }
        if (err == SELVA_EEXIST) { /* Default handling */
            selva_send_str(resp, "OK", 2);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        } else if (err) {
            selva_send_errorf(resp, err, NULL, 0);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (op_code == SELVA_MODIFY_OP_EDGE_META_DEFAULT_DOUBLE ||
               op_code == SELVA_MODIFY_OP_EDGE_META_DOUBLE) {
        double d;

        if (op->meta_field_value_len != sizeof(d)) {
            REPLY_WITH_ARG_TYPE_ERROR(d);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }

        memcpy(&d, op->meta_field_value_str, sizeof(d));

        if (op_code == SELVA_MODIFY_OP_EDGE_META_DEFAULT_DOUBLE) {
            err = SelvaObject_SetDoubleDefaultStr(edge_metadata, op->meta_field_name_str, op->meta_field_name_len, d);
        } else {
            err = SelvaObject_UpdateDoubleStr(edge_metadata, op->meta_field_name_str, op->meta_field_name_len, d);
        }
        if (err == SELVA_EEXIST) { /* Default handling. */
            selva_send_str(resp, "OK", 2);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        } else if (err) {
            selva_send_error(resp, err, NULL, 0);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else if (op_code == SELVA_MODIFY_OP_EDGE_META_DEL) {
        err = SelvaObject_DelKeyStr(edge_metadata, op->meta_field_name_str, op->meta_field_name_len);
        if (err == SELVA_ENOENT) {
            /* No need to replicate. */
            selva_send_str(resp, "OK", 2);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        } else if (err) {
            selva_send_error(resp, err, NULL, 0);
            return SELVA_OP_REPL_STATE_UNCHANGED;
        }
    } else {
        selva_send_error(resp, SELVA_EINTYPE, NULL, 0);
        return SELVA_OP_REPL_STATE_UNCHANGED;
    }

    return SELVA_OP_REPL_STATE_UPDATED;
}

static void replicate_modify(struct selva_server_response_out *resp, const struct bitmap *replset, struct selva_string **orig_argv, const struct replicate_ts *rs)
{
    const int leading_args = 2; /* [key, flags] */
    const long long count = bitmap_popcount(replset);
    struct selva_proto_builder_msg msg;

    if (count == 0 && !rs->created && !rs->updated) {
        return; /* Skip. */
    }

    selva_proto_builder_init(&msg);

    /*
     * Insert the leading args.
     */
    for (int i = 0; i < leading_args; i++) {
        size_t len;
        const char *str = selva_string_to_str(orig_argv[i], &len);

        selva_proto_builder_insert_string(&msg, str, len);
    }

    /*
     * Insert changes.
     */
    int i_arg_type = leading_args;
    for (int i = 0; i < (int)replset->nbits; i++) {
        if (bitmap_get(replset, i)) {
            for (int j = 0; j < 3; j++) {
                size_t len;
                const char *str = selva_string_to_str(orig_argv[i_arg_type + j], &len);

                selva_proto_builder_insert_string(&msg, str, len);
            }
        }
        i_arg_type += 3;
    }

    /*
     * Make sure created_at field is always in sync on all nodes.
     */
    if (rs->created) {
        const char op[2] = { SELVA_MODIFY_ARG_LONGLONG, '\0' };
        size_t size = sizeof(rs->created_at);
        char value[size];

        memcpy(value, &rs->created_at, size);
        selva_proto_builder_insert_string(&msg, op, sizeof(op) - 1);
        selva_proto_builder_insert_string(&msg, SELVA_CREATED_AT_FIELD, sizeof(SELVA_CREATED_AT_FIELD) - 1);
        selva_proto_builder_insert_string(&msg, value, size);
    }

    /*
     * Make sure updated_at field is always in sync on all nodes.
     */
    if (rs->updated) {
        const char op[2] = { SELVA_MODIFY_ARG_LONGLONG, '\0' };
        size_t size = sizeof(rs->updated_at);
        char value[size];

        memcpy(value, &rs->updated_at, size);
        selva_proto_builder_insert_string(&msg, op, sizeof(op) - 1);
        selva_proto_builder_insert_string(&msg, SELVA_UPDATED_AT_FIELD, sizeof(SELVA_UPDATED_AT_FIELD) - 1);
        selva_proto_builder_insert_string(&msg, value, size);
    }

    /*
     * It's kinda not optimal needing to check this global on every replication
     * but it helps us with testing and it's still very negligible overhead.
     * TODO Perhaps in the future we could have a CI build flag to enable this
     * sort of things.
     */
    if (selva_glob_config.debug_modify_replication_delay_ns > 0) {
        const struct timespec tim = {
            .tv_sec = 0,
            .tv_nsec = selva_glob_config.debug_modify_replication_delay_ns,
        };

        nanosleep(&tim, NULL);
    }

    selva_proto_builder_end(&msg);
    selva_replication_replicate_pass(selva_resp_to_cmd_id(resp), msg.buf, msg.bsize);
    /*
     * Deinit can be omitted because selva_replication_replicate_pass() will
     * free the buffer.
     */
#if 0
    selva_proto_builder_deinit(&msg);
#endif
}

/*
 * Request:
 * {node_id, FLAGS, type, field, value [, ... type, field, value]]}
 * N = No root
 * M = Merge
 *
 * The behavior and meaning of `value` depends on `type` (enum SelvaModify_ArgType).
 *
 * Response:
 * [
 * id,
 * [err | 0 | 1]
 * ...
 * ]
 *
 * err = error in parsing or executing the triplet
 * OK = the triplet made no changes
 * UPDATED = changes made and replicated
 */
void SelvaCommand_Modify(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SELVA_TRACE_BEGIN_AUTO(cmd_modify);
    __auto_finalizer struct finalizer fin;
    SelvaHierarchy *hierarchy = main_hierarchy;
    struct selva_string **argv;
    int argc;
    SVECTOR_AUTOFREE(alias_query);
    bool created = false; /* Will be set if the node was created during this command. */
    bool updated = false;
    bool new_alias = false; /* Set if $alias will be creating new alias(es). */
    int err = 0;

    finalizer_init(&fin);

    /*
     * The comparator must be NULL to ensure that the vector is always stored
     * as an array as that is required later on for the modify op.
     */
    SVector_Init(&alias_query, 5, NULL);

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    } else if (argc < 5 || (argc - 2) % 3) {
        /*
         * We expect two fixed arguments and a number of [type, field, value] triplets.
         */
        selva_send_error_arity(resp);
        return;
    }

    /*
     * We use the ID generated by the client as the nodeId by default but later
     * on if an $alias entry is found then the following value will be discarded.
     */
    Selva_NodeId nodeId;
    err = selva_string2node_id(nodeId, argv[0]);
    if (err) {
        selva_send_errorf(resp, err, "Invalid nodeId");
        return;
    }

    /*
     * Look for $alias that would replace id.
     */
    parse_alias_query(argv + 2, argc - 2, &alias_query);
    if (SVector_Size(&alias_query) > 0) {
        struct SVectorIterator it;
        char *str;

        /*
         * Replace id with the first match from alias_query.
         */
        SVector_ForeachBegin(&it, &alias_query);
        while ((str = SVector_Foreach(&it))) {
            Selva_NodeId tmp_id;

            if (!get_alias_str(hierarchy, str, strlen(str), tmp_id)) {
                if (SelvaHierarchy_NodeExists(hierarchy, tmp_id)) {
                    memcpy(nodeId, tmp_id, SELVA_NODE_ID_SIZE);

                    /*
                     * If no match was found all the aliases should be assigned.
                     * If a match was found the query vector can be cleared now
                     * to prevent any new aliases from being created.
                     */
                    SVector_Clear(&alias_query);

                    break;
                }
            }
        }
    }
    if (SVector_Size(&alias_query) > 0) {
            new_alias = true;
    }

    struct SelvaHierarchyNode *node;
    const unsigned flags = parse_flags(argv[1]);

    node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        if (FISSET_UPDATE(flags)) {
            /* if the specified id doesn't exist but $operation: 'update' specified */
            selva_send_null(resp);
            return;
        }

        const size_t nr_parents = !FISSET_NO_ROOT(flags);

        err = SelvaModify_SetHierarchy(hierarchy, nodeId, nr_parents, ((Selva_NodeId []){ ROOT_NODE_ID }), 0, NULL, &node);
        if (err < 0) {
            selva_send_errorf(resp, err,"ERR Failed to initialize the node hierarchy for id: \"%.*s\"", (int)SELVA_NODE_ID_SIZE, nodeId);
            return;
        }
    } else if (FISSET_CREATE(flags)) {
        /* if the specified id exists but $operation: 'insert' specified. */
        selva_send_null(resp);
        return;
    }

    created = updated = SelvaHierarchy_ClearNodeFlagImplicit(node);
    SelvaSubscriptions_FieldChangePrecheck(hierarchy, node);

    if (!created && FISSET_NO_MERGE(flags)) {
        SelvaHierarchy_ClearNodeFields(SelvaHierarchy_GetNodeObject(node));
    }

    /*
     * Replication bitmap.
     *
     * bit  desc
     * 0    replicate the first triplet
     * 1    replicate the second triplet
     * ...  ...
     */
    const int nr_triplets = (argc - 2) / 3;
    struct bitmap *replset = selva_calloc(1, BITMAP_ALLOC_SIZE(nr_triplets));

    finalizer_add(&fin, replset, selva_free);
    replset->nbits = nr_triplets;
    bitmap_erase(replset);

    int has_push = 0;
    int active_insert_idx = -1;

    /*
     * Parse the rest of the arguments and run the modify operations.
     * Each part of the command will send a separate response back to the client.
     * Each part is also replicated separately.
     */
    selva_send_array(resp, 1 + nr_triplets);
    selva_send_str(resp, nodeId, SELVA_NODE_ID_SIZE);

    for (int i = 2; i < argc; i += 3) {
        struct selva_string *type = argv[i];
        struct selva_string *field = argv[i + 1];
        struct selva_string *value = argv[i + 2];
        TO_STR(type, field);
        const char type_code = type_str[0]; /* [0] always points to a valid char in RM_String. */
        enum selva_op_repl_state repl_state = SELVA_OP_REPL_STATE_UNCHANGED;

        if (get_array_field_index(field_str, field_len, NULL) >= 0) {
            repl_state = modify_array_op(&fin, resp, node, &active_insert_idx, has_push, type_code, field, value);
        } else if (type_code == SELVA_MODIFY_ARG_OP_ARRAY_PUSH) {
            TO_STR(value);
            uint32_t item_type;

            if (value_len != sizeof(uint32_t)) {
                selva_send_errorf(resp, SELVA_EINVAL, "Invalid item type");
                continue;
            }

            memcpy(&item_type, value_str, sizeof(uint32_t));
            if (item_type == SELVA_OBJECT_OBJECT) {
                struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
                struct SelvaObject *new_obj;
                int err;

                new_obj = SelvaObject_New();
                err = SelvaObject_InsertArrayStr(obj, field_str, field_len, SELVA_OBJECT_OBJECT, new_obj);
                if (err) {
                    selva_send_errorf(resp, err, "Failed to push new object to array index: %.*s[]",
                                      (int)field_len, field_str);
                    continue;
                }
            } else {
                has_push = 1;
            }

            repl_state = SELVA_OP_REPL_STATE_UPDATED;
        } else if (type_code == SELVA_MODIFY_ARG_OP_ARRAY_INSERT) {
            TO_STR(value);
            uint32_t item_type;
            uint32_t insert_idx;

            if (value_len != 2 * sizeof(uint32_t)) {
                selva_send_errorf(resp, SELVA_EINTYPE, "Expected: int[2]");
                continue;
            }

            memcpy(&item_type, value_str, sizeof(uint32_t));
            memcpy(&insert_idx, value_str + sizeof(uint32_t), sizeof(uint32_t));

            if (item_type == SELVA_OBJECT_OBJECT) {
                struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
                struct SelvaObject *new_obj = SelvaObject_New();
                int err;

                err = SelvaObject_InsertArrayIndexStr(obj, field_str, field_len, SELVA_OBJECT_OBJECT, insert_idx, new_obj);
                if (err) {
                    SelvaObject_Destroy(new_obj);

                    selva_send_errorf(resp, err, "Failed to push new object to array index: %.*s",
                                      (int)field_len, field_str);
                    continue;
                }
            } else {
                active_insert_idx = insert_idx;
            }

            repl_state = SELVA_OP_REPL_STATE_UPDATED;
        } else if (type_code == SELVA_MODIFY_ARG_OP_ARRAY_QUEUE_TRIM) {
            TO_STR(value);
            uint32_t max_array_len;

            if (value_len != 1 * sizeof(uint32_t)) {
                selva_send_errorf(resp, SELVA_EINTYPE, "Expected: int[1]");
                continue;
            }

            memcpy(&max_array_len, value_str, sizeof(uint32_t));

            struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
            const uint32_t ary_len = (uint32_t)SelvaObject_GetArrayLenStr(obj, field_str, field_len);
            if (ary_len > max_array_len) {
                for (uint32_t i = ary_len; i > max_array_len; i--) {
                    int err;

                    err = SelvaObject_RemoveArrayIndexStr(obj, field_str, field_len, i - 1);
                    if (err) {
                        SELVA_LOG(SELVA_LOGL_ERR, "Failed to remove array index (%.*s[%d]): %s",
                                  (int)field_len, field_str,
                                  (int)i - 1,
                                  selva_strerror(err));
                    }
                }
            }

            repl_state = SELVA_OP_REPL_STATE_UPDATED;
        } else if (type_code == SELVA_MODIFY_ARG_STRING_ARRAY) {
            /*
             * Currently the $alias query is the only operation using string arrays.
             * $alias: NOP
             */
            if (new_alias) {
                selva_send_str(resp, "UPDATED", 7);
            } else {
                selva_send_str(resp, "OK", 2);
            }

            /* This triplet needs to be replicated. */
            bitmap_set(replset, (i - 2) / 3);
            continue;
        } else if (type_code == SELVA_MODIFY_ARG_OP_EDGE_META) {
            repl_state = modify_edge_meta_op(&fin, resp, node, field, value);
        } else {
            repl_state = modify_op(&fin, resp, hierarchy, nodeId, node, type_code, field, value);
        }

        if (repl_state == SELVA_OP_REPL_STATE_REPLICATE) {
            /* This triplet needs to be replicated. */
            bitmap_set(replset, (i - 2) / 3);

            selva_send_str(resp, "OK", 2);
        } else if (repl_state == SELVA_OP_REPL_STATE_UPDATED) {
            /* This triplet needs to be replicated. */
            bitmap_set(replset, (i - 2) / 3);

            /*
             * Publish that the field was changed.
             * Hierarchy handles events for parents and children.
             */
            if (strcmp(field_str, SELVA_PARENTS_FIELD) && strcmp(field_str, SELVA_CHILDREN_FIELD)) {
                SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node, field_str, field_len);
            }

            selva_send_str(resp, "UPDATED", 7);
            updated = true;
        }
    }

    /*
     * If the size of alias_query is greater than zero it means that no match
     * was found for $alias and we need to create all the aliases listed in the
     * query.
     */
    if (SVector_Size(&alias_query) > 0) {
        struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
        struct selva_string *aliases_field = selva_string_create(SELVA_ALIASES_FIELD, sizeof(SELVA_ALIASES_FIELD) - 1, 0);
        struct SVectorIterator it;
        char *alias;

        SVector_ForeachBegin(&it, &alias_query);
        while ((alias = SVector_Foreach(&it))) {
            struct SelvaModify_OpSet opSet = {
                .op_set_type = SELVA_MODIFY_OP_SET_TYPE_CHAR,
                .$add = alias,
                .$add_len = strlen(alias) + 1, /* This is safe because the ultimate source is a selva_string. */
                .$delete = NULL,
                .$delete_len = 0,
                .$value = NULL,
                .$value_len = 0,
            };

            err = SelvaModify_ModifySet(hierarchy, nodeId, node, obj, aliases_field, &opSet);
            if (err < 0) {
                /*
                 * Since we are already at the end of the command, it's next to
                 * impossible to rollback the command, so we'll just log any
                 * errors received here.
                 */
                SELVA_LOG(SELVA_LOGL_ERR, "An error occurred while setting an alias \"%s\" -> %.*s: %s\n",
                          alias,
                          (int)SELVA_NODE_ID_SIZE, nodeId,
                          selva_strerror(err));
            }
        }

        selva_string_free(aliases_field);
    }

    if (created) {
        SelvaSubscriptions_DeferTriggerEvents(hierarchy, node, SELVA_SUBSCRIPTION_TRIGGER_TYPE_CREATED);
    }
    if (updated && !created) {
        /*
         * If nodeId wasn't created by this command call but it was updated
         * then we need to defer the updated trigger.
         */
        SelvaSubscriptions_DeferTriggerEvents(hierarchy, node, SELVA_SUBSCRIPTION_TRIGGER_TYPE_UPDATED);

        /* FIXME Replication */
        if (1 /* !(RedisModule_GetContextFlags() & REDISMODULE_CTX_FLAGS_REPLICATED) */) {
            struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
            const long long now = ts_now();

            /*
             * If the node was created then the field was already updated by hierarchy.
             * If the command was replicated then the master should send us the correct
             * timestamp.
             */
            SelvaObject_SetLongLongStr(obj, SELVA_UPDATED_AT_FIELD, sizeof(SELVA_UPDATED_AT_FIELD) - 1, now);
            SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node, SELVA_UPDATED_AT_FIELD, sizeof(SELVA_UPDATED_AT_FIELD) - 1);
        }
    }

    selva_db_is_dirty = 1; /* TODO Not necessarily modified. */

    if (selva_replication_get_mode() == SELVA_REPLICATION_MODE_ORIGIN) {
        struct replicate_ts replicate_ts;

        get_replicate_ts(&replicate_ts, node, created, updated);
        replicate_modify(resp, replset, argv, &replicate_ts);
    }

    SelvaSubscriptions_SendDeferredEvents(hierarchy);

    return;
}

static int Modify_OnLoad(void) {
    selva_mk_command(CMD_MODIFY_ID, SELVA_CMD_MODE_MUTATE, "modify", SelvaCommand_Modify);

    return 0;
}
SELVA_ONLOAD(Modify_OnLoad);
