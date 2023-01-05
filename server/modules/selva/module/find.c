/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <tgmath.h>
#include "redismodule.h"
#include "selva.h"
#include "jemalloc.h"
#include "auto_free.h"
#include "arg_parser.h"
#include "ptag.h"
#include "hierarchy.h"
#include "rpn.h"
#include "config.h"
#include "selva_lang.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "selva_trace.h"
#include "subscriptions.h"
#include "edge.h"
#include "svector.h"
#include "cstrings.h"
#include "traversal.h"
#include "inherit.h"
#include "find_index.h"

#define WILDCARD_CHAR '*'

struct FindCommand_ArrayObjectCb {
    RedisModuleCtx *ctx;
    struct FindCommand_Args *find_args;
};

/*
 * Trace handles.
 */
SELVA_TRACE_HANDLE(cmd_find_array);
SELVA_TRACE_HANDLE(cmd_find_bfs_expression);
SELVA_TRACE_HANDLE(cmd_find_index);
SELVA_TRACE_HANDLE(cmd_find_refs);
SELVA_TRACE_HANDLE(cmd_find_rest);
SELVA_TRACE_HANDLE(cmd_find_sort_result);
SELVA_TRACE_HANDLE(cmd_find_traversal_expression);

static int send_node_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_prefix_str,
        size_t field_prefix_len,
        const char *field_str,
        size_t field_len,
        RedisModuleString *excluded_fields);
static int send_all_node_data_fields(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *excluded_fields);

static int send_hierarchy_field(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId nodeId,
        const char *full_field_name_str,
        size_t full_field_name_len,
        const char *field_str,
        size_t field_len) {
#define SEND_FIELD_NAME() RedisModule_ReplyWithStringBuffer(ctx, full_field_name_str, full_field_name_len)
#define IS_FIELD(name) \
    (field_len == (sizeof(name) - 1) && !memcmp(field_str, name, sizeof(name) - 1))

    /*
     * Check if the field name is a hierarchy field name.
     * We use length check and memcmp() here instead of strcmp() because it
     * seems to give us a much better branch prediction success rate and
     * this function is pretty hot.
     */
    if (IS_FIELD(SELVA_ANCESTORS_FIELD)) {
        SEND_FIELD_NAME();
        return HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS);
    } else if (IS_FIELD(SELVA_CHILDREN_FIELD)) {
        SEND_FIELD_NAME();
        return HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_CHILDREN);
    } else if (IS_FIELD(SELVA_DESCENDANTS_FIELD)) {
        SEND_FIELD_NAME();
        return HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS);
    } else if (IS_FIELD(SELVA_PARENTS_FIELD)) {
        SEND_FIELD_NAME();
        return HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_PARENTS);
    }

    return SELVA_ENOENT;
#undef SEND_FIELD_NAME
#undef IS_FIELD
}

static int iswildcard(const char *field_str, size_t field_len) {
    return field_len == 1 && field_str[0] == WILDCARD_CHAR;
}

static int send_edge_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_prefix_str,
        size_t field_prefix_len,
        const char *field_str,
        size_t field_len,
        RedisModuleString *excluded_fields) {
    struct SelvaHierarchyMetadata *metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);
    struct SelvaObject *edges = metadata->edge_fields.edges;
    struct EdgeField *edge_field;
    void *p;

    if (!edges) {
        return SELVA_ENOENT;
    }

#if 0
    if (field_len > 2 && field_str[field_len - 2] == '.' && field_str[field_len - 1] == '*') {
        field_len -= 2;
    } else if (memmem(field_str, field_len, ".*.", 3)) {
#endif
    if (memmem(field_str, field_len, ".*.", 3)) {
        long resp_count = 0;
        int err;

        err = SelvaObject_ReplyWithWildcardStr(ctx, NULL, edges, field_str, field_len, &resp_count, 0, SELVA_OBJECT_REPLY_ANY_OBJ_FLAG);
        if (err && err != SELVA_ENOENT) {
            Selva_NodeId node_id;

            SELVA_LOG(SELVA_LOGL_ERR, "Sending edge fields with a wildcard \"%.*s\" of %.*s failed: %s",
                      (int)field_len, field_str,
                      (int)SELVA_NODE_ID_SIZE, SelvaHierarchy_GetNodeId(node_id, node),
                      getSelvaErrorStr(err));
        }
        return (int)(resp_count / 2);
    }

    const int off = SelvaObject_GetPointerPartialMatchStr(edges, field_str, field_len, &p);
    edge_field = p;
    if (off == SELVA_EINTYPE) {
        struct SelvaObject *next_obj;
        SelvaObject_Iterator *it;
        const char *key;
        int res = 0;

        if (iswildcard(field_str, field_len)) {
            field_len = 0;
        }

        if (SelvaObject_GetObjectStr(edges, field_str, field_len, &next_obj)) {
            /* Fail if type wasn't SELVA_OBJECT_OBJECT */
            return off;
        }

        it = SelvaObject_ForeachBegin(next_obj);
        while ((key = SelvaObject_ForeachKey(edges, &it))) {
            const size_t next_field_len = field_len + 1 + strlen(key);
            char next_field_str[next_field_len + 1];
            int err;

            snprintf(next_field_str, next_field_len + 1, "%.*s.%s", (int)field_len, field_str, key);
            err = send_edge_field(ctx, lang, hierarchy, node, field_prefix_str, field_prefix_len, next_field_str, next_field_len, excluded_fields);
            if (err >= 0) {
                res += err;
            } else {
                /* TODO What to do in case of an error? */
            }
        }

        return res;
    } else if (off < 0) {
        return off;
    } else if (!edge_field) {
        return SELVA_ENOENT;
    } else if (off == 0) {
        if (field_prefix_str) {
            RedisModuleString *act_field_name;

            act_field_name = RedisModule_CreateStringPrintf(ctx, "%.*s%s", (int)field_prefix_len, field_prefix_str, field_str);
            RedisModule_ReplyWithString(ctx, act_field_name);
        } else {
            RedisModule_ReplyWithStringBuffer(ctx, field_str, field_len);
        }

        replyWithEdgeField(ctx, edge_field);
        return 1;
    } else {
        /*
         * Note: The dst_node might be the same as node but this shouldn't case
         * an infinite loop or any other issues as we'll be always cutting the
         * field name shorter and thus the recursion should eventually stop.
         */
        const size_t nr_arcs = Edge_GetFieldLength(edge_field);

        /*
         * RFE Historically we have been sending ENOENT but is that a good practice?
         */
        if (nr_arcs == 0) {
            return SELVA_ENOENT;
        }

        RedisModule_ReplyWithStringBuffer(ctx, field_str, off - 1);
        RedisModule_ReplyWithArray(ctx, nr_arcs);

        const char *next_field_str = field_str + off;
        const size_t next_field_len = field_len - off;
        const int is_wildcard = iswildcard(next_field_str, next_field_len);

        const char *next_prefix_str;
        size_t next_prefix_len;

        if (field_prefix_str) {
            const char *s = memmem(field_str, field_len, ".", 1);
            const int n = s ? (int)(s - field_str) + 1 : (int)field_len;
            const RedisModuleString *next_prefix;

            /* RFE Handle error? */
            next_prefix = RedisModule_CreateStringPrintf(ctx, "%.*s%.*s", (int)field_prefix_len, field_prefix_str, n, field_str);
            next_prefix_str = RedisModule_StringPtrLen(next_prefix, &next_prefix_len);
        } else {
            /*
             * Don't add prefix because we are sending multiple nested objects
             * in an array.
             */
            next_prefix_str = NULL;
            next_prefix_len = 0;
        }

        struct SVectorIterator it;
        struct SelvaHierarchyNode *dst_node;

        Edge_ForeachBegin(&it, edge_field);
        while ((dst_node = Edge_Foreach(&it))) {
            int nr_fields = 1;
            Selva_NodeId dst_node_id;

            SelvaHierarchy_GetNodeId(dst_node_id, dst_node);

            RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

            /*
             * Id field is always sent to provide a context for typeCast.
             */
            RedisModule_ReplyWithStringBuffer(ctx, SELVA_ID_FIELD, sizeof(SELVA_ID_FIELD) - 1);
            RedisModule_ReplyWithStringBuffer(ctx, dst_node_id, Selva_NodeIdLen(dst_node_id));

            RedisModuleString *new_excluded_fields = NULL;
            if (excluded_fields) {
                TO_STR(excluded_fields);
                size_t field_stop;
                char new_excluded_fields_str[excluded_fields_len + 1];
                size_t new_excluded_fields_len;

                if (is_wildcard) {
                    field_stop = field_len - 1;
                } else {
                    const char *s = memchr(field_str, '.', field_len);

                    if (s) {
                        field_stop = (size_t)(s - field_str + 1);
                    } else {
                        /* RFE is this a case? */
                        field_stop = field_len;
                    }
                }

                stringlist_remove_prefix(new_excluded_fields_str, excluded_fields_str, (int)excluded_fields_len, field_str, field_stop);
                new_excluded_fields_len = strlen(new_excluded_fields_str);

                if (new_excluded_fields_len > 0) {
                    new_excluded_fields = RedisModule_CreateString(ctx, new_excluded_fields_str, new_excluded_fields_len);
                }
            }

            if (is_wildcard) {
                int res;

                if (next_prefix_str) {
                    RedisModule_ReplyWithArray(ctx, 2);
                    RedisModule_ReplyWithStringBuffer(ctx, next_prefix_str, next_prefix_len - 1);
                    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
                    nr_fields++;
                }

                res = send_all_node_data_fields(ctx, lang, hierarchy, dst_node, NULL, 0, new_excluded_fields);
                if (next_prefix_str) {
                    RedisModule_ReplySetArrayLength(ctx, res > 0 ? res : 0);
                } else if (res >= 0) {
                    nr_fields += res;
                }
            } else {
                struct SelvaObject *dst_obj = SelvaHierarchy_GetNodeObject(dst_node);
                int res;

                res = send_node_field(ctx, lang, hierarchy, dst_node, dst_obj,
                                      next_prefix_str, next_prefix_len,
                                      next_field_str, next_field_len,
                                      new_excluded_fields);
                if (res > 0) {
                    nr_fields += res;
                }
            }

            RedisModule_ReplySetArrayLength(ctx, 2 * nr_fields);
        }

        return 1;
    }
    /* NOT REACHED */
}

/**
 * Send a node field to the client.
 * @param excluded_fields can be set if certain field names should be excluded from the response; Otherwise NULL.
 */
static int send_node_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_prefix_str,
        size_t field_prefix_len,
        const char *field_str,
        size_t field_len,
        RedisModuleString *excluded_fields) {
    Selva_NodeId nodeId;
    const char *full_field_name_str;
    size_t full_field_name_len;
    int err;

    SelvaHierarchy_GetNodeId(nodeId, node);

    if (field_prefix_str) {
        RedisModuleString *full_field_name;

        full_field_name = RedisModule_CreateStringPrintf(ctx, "%.*s%.*s", (int)field_prefix_len, field_prefix_str, (int)field_len, field_str);
        full_field_name_str = RedisModule_StringPtrLen(full_field_name, &full_field_name_len);
    } else {
        full_field_name_str = field_str;
        full_field_name_len = field_len;
    }

    if (excluded_fields) {
        /*
         * Excluding the `id` field is not allowed but we can drop it from the
         * list in the arg_parser. It's not allowed because the client must be
         * able to find the schema for whatever was returned by the server.
         */
#if 0
        && !(full_field_name_len == (sizeof(SELVA_ID_FIELD) - 1) && !memcmp(SELVA_ID_FIELD, full_field_name_str, full_field_name_len))
#endif
        TO_STR(excluded_fields);

        if (stringlist_searchn(excluded_fields_str, full_field_name_str, full_field_name_len)) {
            /*
             * This field should be excluded from the results.
             */
            return 0;
        }
    }

    int res = 0;

    /*
     * Check if the field name is a hierarchy field name.
     */
    err = send_hierarchy_field(ctx, hierarchy, nodeId, full_field_name_str, full_field_name_len, field_str, field_len);
    if (err == 0) {
        return 1;
    } else if (err != SELVA_ENOENT) {
        SELVA_LOG(SELVA_LOGL_ERR, "Sending the %s field of %.*s failed: %s",
                  field_str,
                  (int)SELVA_NODE_ID_SIZE, nodeId,
                  getSelvaErrorStr(err));
        return 1; /* Something was already sent so +1 */
    } else {
        /*
         * Check if the field name is an edge field.
         */
        err = send_edge_field(ctx, lang, hierarchy, node, field_prefix_str, field_prefix_len, field_str, field_len, excluded_fields);
        if (err < 0 && err != SELVA_ENOENT) {
            return 0;
        } else if (err >= 0) {
            res += err;
        }
    }

    /*
     * Check if we have a wildcard in the middle of the field name
     * and process it.
     * TODO Might be better to use memmem()
     */
    if (strstr(field_str, ".*.")) {
        long resp_count = 0;

        err = SelvaObject_ReplyWithWildcardStr(ctx, lang, obj, field_str, field_len, &resp_count, -1, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err && err != SELVA_ENOENT) {
            SELVA_LOG(SELVA_LOGL_ERR, "Sending wildcard field \"%.*s\" of %.*s failed: %s\n",
                      (int)field_len, field_str,
                      (int)SELVA_NODE_ID_SIZE, nodeId,
                      getSelvaErrorStr(err));
        }

        res += (int)(resp_count / 2);
    } else {
        /*
         * Finally check if the field name is a key on the node object.
         */

        if (field_len >= 2 && field_str[field_len - 2] == '.' && field_str[field_len - 1] == '*') {
            field_len -= 2;
        }

        if (SelvaObject_ExistsStr(obj, field_str, field_len)) {
            /* Field didn't exist in the node. */
            return res;
        }

        /*
         * Send the reply.
         */
        RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "%.*s%.*s", (int)field_prefix_len, field_prefix_str, (int)field_len, field_str));
        err = SelvaObject_ReplyWithObjectStr(ctx, lang, obj, field_str, field_len, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to send the field (%.*s) for node_id: \"%.*s\" err: \"%s\"",
                      (int)field_len, field_str,
                      (int)SELVA_NODE_ID_SIZE, nodeId,
                      getSelvaErrorStr(err));
            RedisModule_ReplyWithNull(ctx);
        }

        res++;
    }

    return res;
}

/**
 * @param excluded_fields can be set if certain field names should be excluded from the response; Otherwise NULL.
 */
static int send_all_node_data_fields(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *excluded_fields) {
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    void *iterator;
    const char *field_name_str;
    int nr_fields = 0;

    iterator = SelvaObject_ForeachBegin(obj);
    while ((field_name_str = SelvaObject_ForeachKey(obj, &iterator))) {
        size_t field_name_len = strlen(field_name_str);
        int res;

        res = send_node_field(ctx, lang, hierarchy, node, obj, field_prefix_str, field_prefix_len, field_name_str, field_name_len, excluded_fields);
        if (res >= 0) {
            nr_fields += res;
        } else {
            /* RFE errors are ignored for now. */
            Selva_NodeId node_id;

            SelvaHierarchy_GetNodeId(node_id, node);
            SELVA_LOG(SELVA_LOGL_ERR, "send_node_field(%.*s, %.*s) failed: %s\n",
                      (int)SELVA_NODE_ID_SIZE, node_id,
                      (int)field_name_len, field_name_str,
                      getSelvaErrorStr(res));
        }
    }

    return nr_fields;
}

/**
 * Send named fields.
 * Should be only used by send_node_fields().
 */
static size_t send_node_fields_named(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaObject *fields,
        RedisModuleString *excluded_fields) {
    struct SelvaObject *obj = SelvaHierarchy_GetNodeObject(node);
    void *iterator;
    const SVector *vec;
    size_t nr_fields = 0;

    iterator = SelvaObject_ForeachBegin(fields);
    while ((vec = SelvaObject_ForeachValue(fields, &iterator, NULL, SELVA_OBJECT_ARRAY))) {
        struct SVectorIterator it;
        RedisModuleString *field;

        SVector_ForeachBegin(&it, vec);
        while ((field = SVector_Foreach(&it))) {
            TO_STR(field);
            int res;

            if (field_len == 1 && field_str[0] == WILDCARD_CHAR) {
                res = send_all_node_data_fields(ctx, lang, hierarchy, node, NULL, 0, excluded_fields);
                if (res > 0) {
                    nr_fields += res;
                    /*
                     * An interesting case here is a list like this:
                     * `title\n*`
                     * that would send the same field twice.
                     */
                    break;
                }
            } else {
                res = send_node_field(ctx, lang, hierarchy, node, obj, NULL, 0, field_str, field_len, excluded_fields);
                if (res > 0) {
                    nr_fields += res;
                    break; /* Only send one of the fields in the list. */
                }
            }
        }
    }

    return nr_fields;
}

static size_t count_inherit_fields(RedisModuleStringList inherit_fields) {
    size_t nr_inherit_fields;
    RedisModuleStringList s = inherit_fields;

    /*
     * Counting the fields is easy because we know the list ends with
     * a NULL pointer.
     */
    while (*s++);
    nr_inherit_fields = s - inherit_fields - 1;

    return nr_inherit_fields;
}

/**
 * Send node fields to the client.
 */
static int send_node_fields(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaObject *fields,
        RedisModuleStringList inherit_fields,
        RedisModuleString *excluded_fields) {
    Selva_NodeId nodeId;

    SelvaHierarchy_GetNodeId(nodeId, node);

    /*
     * The response format:
     * ```
     *   [
     *     nodeId,
     *     [
     *       fieldName1,
     *       fieldValue1,
     *       fieldName2,
     *       fieldValue2,
     *       ...
     *       fieldNameN,
     *       fieldValueN,
     *     ]
     *   ]
     * ```
     */

    RedisModule_ReplyWithArray(ctx, 2);
    RedisModule_ReplyWithStringBuffer(ctx, nodeId, Selva_NodeIdLen(nodeId));

    const ssize_t fields_len = SelvaObject_Len(fields, NULL);
    if (fields_len < 0) {
        return fields_len;
    } else {
        size_t nr_fields = 0;

        RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
        nr_fields += send_node_fields_named(ctx, lang, hierarchy, node, fields, excluded_fields);
        if (inherit_fields) {
            /*
             * This should only happen in the postprocess handling because
             * otherwise we'll easily hit the reentrancy limit of the trx
             * system.
             */

            size_t nr_inherit_fields = count_inherit_fields(inherit_fields);
            nr_fields += Inherit_SendFields(ctx, hierarchy, lang,
                                            nodeId,
                                            inherit_fields, nr_inherit_fields);
        }
        RedisModule_ReplySetArrayLength(ctx, 2 * nr_fields);
    }

    return 0;
}

static int send_array_object_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        struct SelvaObject *obj,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *field) {
    TO_STR(field);
    int err;

    RedisModuleString *full_field_name;
    if (field_prefix_str) {
        full_field_name = RedisModule_CreateStringPrintf(ctx, "%.*s%s", (int)field_prefix_len, field_prefix_str, field_str);
    } else {
        full_field_name = field;
    }

    /*
     * Check if we have a wildcard in the middle of the field name
     * and process it.
     */
    if (strstr(field_str, ".*.")) {
        long resp_count = 0;

        err = SelvaObject_ReplyWithWildcardStr(ctx, lang, obj, field_str, field_len, &resp_count, -1, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err && err != SELVA_ENOENT) {
            SELVA_LOG(SELVA_LOGL_ERR, "Sending wildcard field %.*s in array object failed: %s\n",
                      (int)field_len, field_str,
                      getSelvaErrorStr(err));
        }

        return (int)(resp_count / 2);
    }

    /*
     * Finally check if the field name is a key on the node object.
     */
    if (SelvaObject_Exists(obj, field)) {
        /* Field didn't exist in the node. */
        return 0;
    }

    /*
     * Send the reply.
     */
    RedisModule_ReplyWithString(ctx, full_field_name);
    err = SelvaObject_ReplyWithObject(ctx, lang, obj, field, SELVA_OBJECT_REPLY_BINUMF_FLAG);
    if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to send the field (%s) in array object err: \"%s\"\n",
                  field_str,
                  getSelvaErrorStr(err));
        RedisModule_ReplyWithNull(ctx);
    }

    return 1;
}

static int send_array_object_fields(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        struct SelvaObject *obj,
        struct SelvaObject *fields) {
    const char wildcard[2] = { WILDCARD_CHAR, '\0' };
    int err;

    /*
     * The response format:
     * ```
     *   [
     *     nodeId,
     *     [
     *       fieldName1,
     *       fieldValue1,
     *       fieldName2,
     *       fieldValue2,
     *       ...
     *       fieldNameN,
     *       fieldValueN,
     *     ]
     *   ]
     * ```
     */

    RedisModule_ReplyWithArray(ctx, 2);
    RedisModule_ReplyWithStringBuffer(ctx, EMPTY_NODE_ID, SELVA_NODE_ID_SIZE);

    const ssize_t fields_len = SelvaObject_Len(fields, NULL);
    if (fields_len < 0) {
        return fields_len;
    } else if (fields_len == 1 &&
               SelvaTraversal_FieldsContains(fields, wildcard, sizeof(wildcard) - 1)) {
        err = SelvaObject_ReplyWithObject(ctx, lang, obj, NULL, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to send all fields for selva object in array: %s\n",
                      getSelvaErrorStr(err));
        }
    } else {
        void *iterator;
        const SVector *vec;
        size_t nr_fields = 0;

        RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

        iterator = SelvaObject_ForeachBegin(fields);
        while ((vec = SelvaObject_ForeachValue(fields, &iterator, NULL, SELVA_OBJECT_ARRAY))) {
            struct SVectorIterator it;
            RedisModuleString *field;

            SVector_ForeachBegin(&it, vec);
            while ((field = SVector_Foreach(&it))) {
                int res;

                res = send_array_object_field(ctx, lang, obj, NULL, 0, field);
                if (res <= 0) {
                    continue;
                } else {
                    nr_fields += res;
                    break; /* Only send one of the fields in the list. */
                }
            }
        }

        RedisModule_ReplySetArrayLength(ctx, 2 * nr_fields);
    }

    return 0;
}

/**
 * @param path_str is the prefix.
 * @param key_name_str is the key name in the current object.
 */
static RedisModuleString *format_full_field_path(RedisModuleCtx *ctx, const char *path_str, const char *key_name_str) {
    RedisModuleString *res;

    if (path_str && path_str[0]) {
        res = RedisModule_CreateStringPrintf(ctx, "%s.%s", path_str, key_name_str);
    } else {
        res = RedisModule_CreateStringPrintf(ctx, "%s", key_name_str);
    }

    return res;
}

static int is_text_field(struct SelvaObject *obj, const char *key_name_str, size_t key_name_len) {
    SelvaObjectMeta_t meta;
    int err;

    err = SelvaObject_GetUserMetaStr(obj, key_name_str, key_name_len, &meta);
    if (err) {
        return 0;
    }

    return meta == SELVA_OBJECT_META_SUBTYPE_TEXT;
}

static int send_merge_text(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        Selva_NodeId nodeId,
        struct SelvaObject *fields,
        struct SelvaObject *obj,
        RedisModuleString *obj_path,
        size_t *nr_fields_out) {
    if (SelvaObject_GetType(fields, obj_path) != SELVA_OBJECT_LONGLONG) {
        int err;

        ++*nr_fields_out;

        /*
         * Start a new array reply:
         * [node_id, field_name, field_value]
         */
        RedisModule_ReplyWithArray(ctx, 3);

        RedisModule_ReplyWithStringBuffer(ctx, nodeId, Selva_NodeIdLen(nodeId));
        RedisModule_ReplyWithString(ctx, obj_path);
        err = SelvaObject_ReplyWithObject(ctx, lang, obj, NULL, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to send \"%s\" (text) of node_id: \"%.*s\": %s\n",
                      RedisModule_StringPtrLen(obj_path, NULL),
                      (int)SELVA_NODE_ID_SIZE, nodeId,
                      getSelvaErrorStr(err));
        } else {
            /* Mark the key as sent. */
            (void)SelvaObject_SetLongLong(fields, obj_path, 1);
        }
    }

    return 0;
}

static int send_merge_all(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        Selva_NodeId nodeId,
        struct SelvaObject *fields,
        struct SelvaObject *obj,
        RedisModuleString *obj_path,
        size_t *nr_fields_out) {
    void *iterator;
    const char *key_name_str;
    TO_STR(obj_path);

    /*
     * Note that the `fields` object is empty in the beginning of the
     * following loop when the send_node_object_merge() function is called for
     * the first time.
     */
    iterator = SelvaObject_ForeachBegin(obj);
    while ((key_name_str = SelvaObject_ForeachKey(obj, &iterator))) {
        const size_t key_name_len = strlen(key_name_str);
        RedisModuleString *full_field_path;
        int err;

        if (!SelvaObject_ExistsStr(fields, key_name_str, strlen(key_name_str))) {
            continue;
        }

        ++*nr_fields_out;
        full_field_path = format_full_field_path(ctx, obj_path_str, key_name_str);

        /*
         * Start a new array reply:
         * [node_id, field_name, field_value]
         */
        RedisModule_ReplyWithArray(ctx, 3);
        RedisModule_ReplyWithStringBuffer(ctx, nodeId, Selva_NodeIdLen(nodeId));
        RedisModule_ReplyWithString(ctx, full_field_path);
        err = SelvaObject_ReplyWithObjectStr(ctx, lang, obj, key_name_str, key_name_len, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err) {
            TO_STR(obj_path);

            SELVA_LOG(SELVA_LOGL_ERR, "Failed to send \"%s.%s\" of node_id: \"%.*s\"\n",
                      obj_path_str,
                      key_name_str,
                      (int)SELVA_NODE_ID_SIZE, nodeId);
            continue;
        }

        /* Mark the key as sent. */
        (void)SelvaObject_SetLongLongStr(fields, key_name_str, key_name_len, 1);
    }

    return 0;
}

static int send_named_merge(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        Selva_NodeId nodeId,
        struct SelvaObject *fields,
        struct SelvaObject *obj,
        RedisModuleString *obj_path,
        size_t *nr_fields_out) {
    void *iterator;
    const SVector *vec;
    const char *field_index;
    TO_STR(obj_path);

    iterator = SelvaObject_ForeachBegin(fields);
    while ((vec = SelvaObject_ForeachValue(fields, &iterator, &field_index, SELVA_OBJECT_ARRAY))) {
        struct SVectorIterator it;
        const RedisModuleString *field;

        SVector_ForeachBegin(&it, vec);
        while ((field = SVector_Foreach(&it))) {
            RedisModuleString *full_field_path;
            int err;

            if (SelvaObject_Exists(obj, field)) {
                continue;
            }

            ++*nr_fields_out;
            full_field_path = format_full_field_path(ctx, obj_path_str, RedisModule_StringPtrLen(field, NULL));

            /*
             * Start a new array reply:
             * [node_id, field_name, field_value]
             */
            RedisModule_ReplyWithArray(ctx, 3);
            RedisModule_ReplyWithStringBuffer(ctx, nodeId, Selva_NodeIdLen(nodeId));
            RedisModule_ReplyWithString(ctx, full_field_path);
            err = SelvaObject_ReplyWithObject(ctx, lang, obj, field, SELVA_OBJECT_REPLY_BINUMF_FLAG);
            if (err) {
                TO_STR(field);

                SELVA_LOG(SELVA_LOGL_ERR, "Failed to send the field (%s) for node_id: \"%.*s\" err: \"%s\"\n",
                          field_str,
                          (int)SELVA_NODE_ID_SIZE, nodeId,
                          getSelvaErrorStr(err));

                /* Reply with null to fill the gap. */
                RedisModule_ReplyWithNull(ctx);
            }

            SelvaObject_DelKeyStr(fields, field_index, strlen(field_index)); /* Remove the field from the list */
            break; /* Only send the first existing field from the fields list. */
        }
    }

    return 0;
}

static int send_deep_merge(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        Selva_NodeId nodeId,
        struct SelvaObject *fields,
        struct SelvaObject *obj,
        RedisModuleString *obj_path,
        size_t *nr_fields_out) {
    void *iterator;
    const char *key_name_str;

    /*
     * Note that the `fields` object is empty in the beginning of the
     * following loop when send_deep_merge() is called for the first time.
     */
    iterator = SelvaObject_ForeachBegin(obj);
    while ((key_name_str = SelvaObject_ForeachKey(obj, &iterator))) {
        const size_t key_name_len = strlen(key_name_str);
        RedisModuleString *next_path;
        enum SelvaObjectType type;
        TO_STR(obj_path);

        next_path = format_full_field_path(ctx, obj_path_str, key_name_str);

        /* Skip fields marked as sent. */
        if (SelvaObject_GetType(fields, next_path) == SELVA_OBJECT_LONGLONG) {
            continue;
        }

        type = SelvaObject_GetTypeStr(obj, key_name_str, key_name_len);
        if (type == SELVA_OBJECT_OBJECT) {
            struct SelvaObject *next_obj;
            int err;

            err = SelvaObject_GetObjectStr(obj, key_name_str, key_name_len, &next_obj);
            if (err) {
                return err;
            }

            err = send_deep_merge(ctx, lang, nodeId, fields, next_obj, next_path, nr_fields_out);
            if (err < 0) {
                SELVA_LOG(SELVA_LOGL_ERR, "Deep merge failed %s",
                          getSelvaErrorStr(err));
            }

            /* Mark the text field as sent. */
            if (is_text_field(obj, key_name_str, key_name_len)) {
                (void)SelvaObject_SetLongLong(fields, next_path, 1);
            }
        } else {
            int err;

            ++*nr_fields_out;

            /*
             * Start a new array reply:
             * [node_id, field_name, field_value]
             */
            RedisModule_ReplyWithArray(ctx, 3);

            RedisModule_ReplyWithStringBuffer(ctx, nodeId, Selva_NodeIdLen(nodeId));
            RedisModule_ReplyWithString(ctx, next_path);
            err = SelvaObject_ReplyWithObjectStr(ctx, lang, obj, key_name_str, key_name_len, SELVA_OBJECT_REPLY_BINUMF_FLAG);
            if (err) {
                TO_STR(obj_path);

                SELVA_LOG(SELVA_LOGL_ERR, "Failed to send \"%s.%s\" of node_id: \"%.*s\"",
                          obj_path_str,
                          key_name_str,
                          (int)SELVA_NODE_ID_SIZE, nodeId);
                continue;
            }

            /* Mark the key as sent. */
            (void)SelvaObject_SetLongLong(fields, next_path, 1);
        }
    }

    return 0;
}

static int send_node_object_merge(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        const struct SelvaHierarchyNode *node,
        enum SelvaMergeStrategy merge_strategy,
        RedisModuleString *obj_path,
        struct SelvaObject *fields,
        size_t *nr_fields_out) {
    Selva_NodeId nodeId;
    struct SelvaObject *node_obj;
    TO_STR(obj_path);
    int err;

    SelvaHierarchy_GetNodeId(nodeId, node);
    node_obj = SelvaHierarchy_GetNodeObject(node);

    /* Get the nested object by given path. */
    struct SelvaObject *obj;
    if (obj_path_len != 0) {
    err = SelvaObject_GetObject(node_obj, obj_path, &obj);
        if (err == SELVA_ENOENT || err == SELVA_EINTYPE) {
            /* Skip this node if the object doesn't exist. */
            return 0;
        } else if (err) {
            return err;
        }
    } else {
        obj = node_obj;
    }

    /*
     * The response format:
     * ```
     *   [
     *     fieldName1,
     *     fieldValue1,
     *     fieldName2,
     *     fieldValue2,
     *     ...
     *     fieldNameN,
     *     fieldValueN,
     *   ]
     * ```
     */
    if ((merge_strategy == MERGE_STRATEGY_ALL || merge_strategy == MERGE_STRATEGY_DEEP) &&
        is_text_field(node_obj, obj_path_str, obj_path_len)) {
        /*
         * If obj is a text field we can just send it directly and skip the rest of
         * the processing.
         */
        err = send_merge_text(ctx, lang, nodeId, fields, obj, obj_path, nr_fields_out);
    } else if (merge_strategy == MERGE_STRATEGY_ALL) {
        /* Send all keys from the nested object. */
        err = send_merge_all(ctx, lang, nodeId, fields, obj, obj_path, nr_fields_out);
    } else if (merge_strategy == MERGE_STRATEGY_NAMED) {
        /* Send named keys from the nested object. */
        err = send_named_merge(ctx, lang, nodeId, fields, obj, obj_path, nr_fields_out);
    } else if (merge_strategy == MERGE_STRATEGY_DEEP) {
        /* Deep merge all keys and nested objects. */
        err = send_deep_merge(ctx, lang, nodeId, fields, obj, obj_path, nr_fields_out);
    } else {
        err = replyWithSelvaErrorf(ctx, SELVA_ENOTSUP, "Merge strategy not supported: %d\n", (int)merge_strategy);
    }

    return err;
}

static int exec_fields_expression(
        struct RedisModuleCtx *redis_ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct rpn_ctx *rpn_ctx,
        const struct rpn_expression *expr,
        struct SelvaObject *fields,
        RedisModuleString ***inherit) {
    Selva_NodeId nodeId;
    struct SelvaSet set;
    enum rpn_error rpn_err;
    struct SelvaSetElement *el;
    size_t i_fields;
    size_t i_inherit;
    RedisModuleString **tmp_inherit_fields = NULL;

    SelvaHierarchy_GetNodeId(nodeId, node);
    rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
    rpn_set_hierarchy_node(rpn_ctx, hierarchy, node);
    rpn_set_obj(rpn_ctx, SelvaHierarchy_GetNodeObject(node));

    SelvaSet_Init(&set, SELVA_SET_TYPE_RMSTRING);
    rpn_err = rpn_selvaset(redis_ctx, rpn_ctx, expr, &set);
    if (rpn_err) {
        /*
         * The exact error code is not important here because it's not passed
         * to the client. EGENERAL is fine enough.
         */
        return SELVA_EGENERAL;
    }

    i_fields = 0;
    i_inherit = 0;
    SELVA_SET_RMS_FOREACH(el, &set) {
        RedisModuleString *field_name = RedisModule_HoldString(NULL, el->value_rms);
        const char *field_name_str = RedisModule_StringPtrLen(el->value_rms, NULL);

        if (field_name_str[0] == '^') { /* inherit */
            tmp_inherit_fields = selva_realloc(tmp_inherit_fields, i_inherit + 1);

            tmp_inherit_fields[i_inherit] = field_name;
            i_inherit++;
        } else { /* no inherit */
            const size_t key_len = (size_t)(log10(i_fields + 1)) + 1;
            char key_str[key_len + 1];

            snprintf(key_str, key_len + 1, "%zu", i_fields);
            SelvaObject_AddArrayStr(fields, key_str, key_len, SELVA_OBJECT_STRING, field_name);
            i_fields++;
        }
    }

    SelvaSet_Destroy(&set);

    *inherit = tmp_inherit_fields;
    return 0;
}

/**
 * Send out given node to the client.
 * The sent data can be one of the following:
 * - a merge result,
 * - selected node fields,
 * - just the node_id.
 */
static void print_node(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
        struct SelvaHierarchyNode *node,
        struct SelvaNodeSendParam *args,
        size_t *merge_nr_fields) {
    int err;

    if (args->merge_strategy != MERGE_STRATEGY_NONE) {
        err = send_node_object_merge(ctx, lang, node, args->merge_strategy, args->merge_path, args->fields, merge_nr_fields);
    } else if (args->fields) { /* Predefined list of fields. */
        err = send_node_fields(ctx, lang, hierarchy, node, args->fields, NULL, args->excluded_fields);
    } else if (args->fields_expression || args->inherit_expression) { /* Select fields using an RPN expression. */
         struct rpn_expression *expr = args->fields_expression ?: args->inherit_expression;
        selvaobject_autofree struct SelvaObject *fields = SelvaObject_New();
         RedisModuleStringList inherit_fields = NULL; /* allocated by exec_fields_expression() */

         assert(!(args->fields_expression && args->inherit_expression));

        err = exec_fields_expression(ctx, hierarchy, node, args->fields_rpn_ctx, expr, fields, &inherit_fields);
        if (!err) {
            err = send_node_fields(ctx, lang, hierarchy, node, fields, inherit_fields, args->excluded_fields);
        }
        if (inherit_fields) {
            selva_free(inherit_fields);
        }
    } else { /* Otherwise the nodeId is sent. */
        Selva_NodeId nodeId;

        SelvaHierarchy_GetNodeId(nodeId, node);
        RedisModule_ReplyWithStringBuffer(ctx, nodeId, Selva_NodeIdLen(nodeId));
        err = 0;
    }

    if (err) {
        Selva_NodeId nodeId;

        RedisModule_ReplyWithNull(ctx);

        SELVA_LOG(SELVA_LOGL_ERR, "Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                  (int)SELVA_NODE_ID_SIZE, SelvaHierarchy_GetNodeId(nodeId, node),
                  getSelvaErrorStr(err));
    }
}

static int process_node_send(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        struct FindCommand_Args *args,
        struct SelvaHierarchyNode *node) {
    ssize_t *nr_nodes = args->nr_nodes;
    ssize_t * restrict limit = args->limit;

    print_node(ctx, hierarchy, args->lang, node, &args->send_param, args->merge_nr_fields);

    *nr_nodes = *nr_nodes + 1;
    *limit = *limit - 1;

    return *limit == 0;
}

static int process_node_sort(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy __unused,
        struct FindCommand_Args *args,
        struct SelvaHierarchyNode *node) {
    struct TraversalOrderItem *item;

    item = SelvaTraversalOrder_CreateNodeOrderItem(ctx, args->lang, node, args->send_param.order_field);
    if (item) {
        SVector_InsertFast(args->result, item);
    } else {
        Selva_NodeId nodeId;

        /*
         * It's not so easy to make the response fail at this point.
         * Given that we shouldn't generally even end up here in real
         * life, it's fairly ok to just log the error and return what
         * we can.
         */
        SelvaHierarchy_GetNodeId(nodeId, node);
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to create an order item for %.*s\n",
                  (int)SELVA_NODE_ID_SIZE, nodeId);
    }

    return 0;
}

static int process_node_inherit(
        RedisModuleCtx *ctx __unused,
        SelvaHierarchy *hierarchy __unused,
        struct FindCommand_Args *args,
        struct SelvaHierarchyNode *node) {
    SVector_Insert(args->result, node);

    return 0;
}

static __hot int FindCommand_NodeCb(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct FindCommand_Args *args = (struct FindCommand_Args *)arg;
    struct rpn_ctx *rpn_ctx = args->rpn_ctx;
    int take = (args->offset > 0) ? !args->offset-- : 1;

    args->acc_tot++;
    if (take && rpn_ctx) {
        Selva_NodeId nodeId;
        int err;

        SelvaHierarchy_GetNodeId(nodeId, node);

        /* Set node_id to the register */
        rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
        rpn_set_hierarchy_node(rpn_ctx, hierarchy, node);
        rpn_set_obj(rpn_ctx, SelvaHierarchy_GetNodeObject(node));

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(ctx, rpn_ctx, args->filter, &take);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Expression failed (node: \"%.*s\"): \"%s\"\n",
                      (int)SELVA_NODE_ID_SIZE, nodeId,
                      rpn_str_error[err]);
            return 1;
        }
    }

    if (take) {
        args->acc_take++;

        return args->process_node(ctx, hierarchy, args, node);
    }

    return 0;
}

static int process_array_obj_send(
        RedisModuleCtx *ctx,
        struct FindCommand_Args *args,
        struct SelvaObject *obj) {
    ssize_t *nr_nodes = args->nr_nodes;
    ssize_t * restrict limit =  args->limit;

    if (args->send_param.fields) {
        int err;

        err = send_array_object_fields(ctx, args->lang, obj, args->send_param.fields);
        if (err) {
            RedisModule_ReplyWithNull(ctx);
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to handle field(s), err: %s",
                      getSelvaErrorStr(err));
        }
    } else {
        RedisModule_ReplyWithStringBuffer(ctx, EMPTY_NODE_ID, SELVA_NODE_ID_SIZE);
    }

    *nr_nodes = *nr_nodes + 1;
    *limit = *limit - 1;

    return *limit == 0;
}

static int process_array_obj_sort(
        RedisModuleCtx *ctx,
        struct FindCommand_Args *args,
        struct SelvaObject *obj) {
    struct TraversalOrderItem *item;

    item = SelvaTraversalOrder_CreateObjectOrderItem(ctx, args->lang, obj, args->send_param.order_field);
    if (item) {
        SVector_InsertFast(args->result, item);
    } else {
        /*
         * It's not so easy to make the response fail at this point.
         * Given that we shouldn't generally even end up here in real
         * life, it's fairly ok to just log the error and return what
         * we can.
         */
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to create an order item");
    }

    return 0;
}

static int FindCommand_ArrayObjectCb(
        union SelvaObjectArrayForeachValue value,
        enum SelvaObjectType subtype,
        void *arg) {
    struct SelvaObject *obj = value.obj;
    struct FindCommand_ArrayObjectCb *args = (struct FindCommand_ArrayObjectCb *)arg;
    struct FindCommand_Args *find_args = args->find_args;
    struct rpn_ctx *rpn_ctx = find_args->rpn_ctx;
    int take = (find_args->offset > 0) ? !find_args->offset-- : 1;

    if (subtype != SELVA_OBJECT_OBJECT) {
        SELVA_LOG(SELVA_LOGL_ERR, "Array subtype not supported: %s",
                  SelvaObject_Type2String(subtype, NULL));
        return 1;
    }

    if (take && rpn_ctx) {
        int err;

        /* Set obj to the register */
        err = rpn_set_reg_slvobj(rpn_ctx, 0, obj, 0);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Register set failed: \"%s\"",
                      rpn_str_error[err]);
            return 1;
        }
        rpn_set_obj(rpn_ctx, obj);

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(args->ctx, rpn_ctx, find_args->filter, &take);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Expression failed: \"%s\"",
                      rpn_str_error[err]);
            return 1;
        }
    }

    if (take) {
        return find_args->process_obj(args->ctx, find_args, obj);
    }

    return 0;
}

/**
 * @param nr_fields_out Only set when merge_strategy != MERGE_STRATEGY_NONE.
 */
static size_t FindCommand_PrintOrderedResult(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
        ssize_t offset,
        ssize_t limit,
        struct SelvaNodeSendParam *args,
        SVector *order_result,
        size_t *merge_nr_fields) {
    struct TraversalOrderItem *item;
    struct SVectorIterator it;
    size_t len = 0;

    /*
     * First handle the offsetting.
     */
    for (ssize_t i = 0; i < offset; i++) {
        SVector_Shift(order_result);
    }
    SVector_ShiftReset(order_result);

    /*
     * Then send out node IDs upto the limit.
     */
    SVector_ForeachBegin(&it, order_result);
    while ((item = SVector_Foreach(&it))) {
        if (limit-- == 0) {
            break;
        }

        assert(PTAG_GETTAG(item->tagp) == TRAVERSAL_ORDER_ITEM_PTYPE_NODE);
        print_node(ctx, hierarchy, lang, PTAG_GETP(item->tagp), args, merge_nr_fields);

        len++;
    }

    return len;
}

static size_t get_nr_out(enum SelvaMergeStrategy merge_strategy, size_t nr_nodes, size_t merge_nr_fields) {
    return (merge_strategy == MERGE_STRATEGY_NONE) ? nr_nodes : merge_nr_fields;
}

static void postprocess_array(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy __unused,
        struct RedisModuleString *lang,
        ssize_t offset,
        ssize_t limit,
        struct SelvaNodeSendParam *args,
        SVector *result) {
    struct TraversalOrderItem *item;
    struct SVectorIterator it;
    size_t len = 0;

    /*
     * First handle the offsetting.
     */
    for (ssize_t i = 0; i < offset; i++) {
        SVector_Shift(result);
    }
    SVector_ShiftReset(result);

    /*
     * Then send out node IDs upto the limit.
     */
    SVector_ForeachBegin(&it, result);
    while ((item = SVector_Foreach(&it))) {
        int err;
        if (limit-- == 0) {
            break;
        }

        assert(PTAG_GETTAG(item->tagp) == TRAVERSAL_ORDER_ITEM_PTYPE_OBJ);
        err = send_array_object_fields(ctx, lang, PTAG_GETP(item->tagp), args->fields);
        if (err) {
            RedisModule_ReplyWithNull(ctx);
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                      (int)SELVA_NODE_ID_SIZE, item->node_id,
                      getSelvaErrorStr(err));
        }

        len++;
    }

    RedisModule_ReplySetArrayLength(ctx, len);
}

/**
 * Send nodes from the result SVector to the client.
 */
static void postprocess_sort(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct RedisModuleString *lang,
        ssize_t offset,
        ssize_t limit,
        struct SelvaNodeSendParam *args,
        SVector *result) {
    size_t nr_nodes;
    size_t merge_nr_fields = 0;

    nr_nodes = FindCommand_PrintOrderedResult(ctx, hierarchy, lang, offset, limit, args, result, &merge_nr_fields);
    RedisModule_ReplySetArrayLength(ctx, get_nr_out(args->merge_strategy, nr_nodes, merge_nr_fields));
}

static void postprocess_inherit(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct RedisModuleString *lang,
        ssize_t offset,
        ssize_t limit,
        struct SelvaNodeSendParam *args,
        SVector *result) {
    /* Merge + inherit == not supported */
    assert(args->merge_strategy == MERGE_STRATEGY_NONE);

    /*
     * If order is set we need to sort the nodes first.
     * In case of inherit we also want to inherit the sort-by field.
     */
    if (args->order != SELVA_RESULT_ORDER_NONE) {
        SVECTOR_AUTOFREE(order_result);
        size_t order_by_field_len;
        const char *order_by_field_str = RedisModule_StringPtrLen(args->order_field, &order_by_field_len);
        struct SelvaHierarchyNode *node;
        struct SVectorIterator it;

        SelvaTraversalOrder_InitOrderResult(&order_result, args->order, limit);

        /* result contains nodes in this case instead of items. */
        assert(!result->vec_compar);

        SVector_ForeachBegin(&it, result);
        while ((node = SVector_Foreach(&it))) {
            Selva_NodeId node_id;
            struct SelvaObjectAny fv;
            struct TraversalOrderItem *item;
            int err;

            SelvaHierarchy_GetNodeId(node_id, node);
            err = Inherit_FieldValue(ctx, hierarchy, lang, node_id, NULL, 0, order_by_field_str, order_by_field_len, &fv);
            if (err) {
                /* TODO Error handling */
                continue;
            }

            item = SelvaTraversalOrder_CreateAnyNodeOrderItem(ctx, node, &fv);
            if (item) {
                SVector_InsertFast(&order_result, item);
            } else {
                Selva_NodeId nodeId;

                SelvaHierarchy_GetNodeId(nodeId, node);
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to create an order item for %.*s\n",
                          (int)SELVA_NODE_ID_SIZE, nodeId);
            }

        }

        RedisModule_ReplySetArrayLength(ctx, FindCommand_PrintOrderedResult(ctx, hierarchy, lang, offset, limit, args, &order_result, NULL));
    } else {
        struct SelvaHierarchyNode *node;
        struct SVectorIterator it;
        size_t nr_nodes = 0;

        /*
         * First handle the offsetting.
         */
        for (ssize_t i = 0; i < offset; i++) {
            SVector_Shift(result);
        }
        SVector_ShiftReset(result);

        /*
         * Then send out node IDs upto the limit.
         */
        SVector_ForeachBegin(&it, result);
        while ((node = SVector_Foreach(&it))) {
            if (limit-- == 0) {
                break;
            }

            print_node(ctx, hierarchy, lang, node, args, NULL);
            nr_nodes++;
        }

        RedisModule_ReplySetArrayLength(ctx, nr_nodes);
    }
}

/**
 * Find node(s) matching the query.
 *
 * SELVA.HIERARCHY.find
 * lang
 * REDIS_KEY
 * dir [field_name/expr]                            Traversal method/direction
 * ["edge_filter" expr]                             Expression to decide whether and edge should be visited
 * ["index" [expr]]                                 Indexing hint
 * ["order" field asc|desc]                         Sort order of the results
 * ["offset" 1234]                                  Skip the first 1234 - 1 results
 * ["limit" 1234]                                   Limit the number of results (Optional)
 * ["merge" path]                                   Merge fields. fields option must be set
 * ["fields(_rpn)|inherit_rpn" field_names|expr]    Return field values instead of node names
 * NODE_IDS                                         One or more node IDs concatenated (10 chars per ID)
 * [expression]                                     RPN filter expression
 * [args...]                                        Register arguments for the RPN filter
 *
 * The traversed field is typically either ancestors or descendants but it can
 * be any hierarchy or edge field.
 */
static int SelvaHierarchy_FindCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_LANG      = 1;
    const int ARGV_REDIS_KEY = 2;
    const int ARGV_DIRECTION = 3;
    const int ARGV_REF_FIELD = 4;
    int ARGV_EDGE_FILTER_TXT = 4;
    int ARGV_EDGE_FILTER_VAL = 5;
    int ARGV_INDEX_TXT       = 4;
    __unused int ARGV_INDEX_VAL = 5;
    int ARGV_ORDER_TXT       = 4;
    int ARGV_ORDER_FLD       = 5;
    int ARGV_ORDER_ORD       = 6;
    int ARGV_OFFSET_TXT      = 4;
    int ARGV_OFFSET_NUM      = 5;
    int ARGV_LIMIT_TXT       = 4;
    int ARGV_LIMIT_NUM       = 5;
    int ARGV_MERGE_TXT       = 4;
    int ARGV_MERGE_VAL       = 5;
    int ARGV_FIELDS_TXT      = 4;
    int ARGV_FIELDS_VAL      = 5;
    int ARGV_NODE_IDS        = 4;
    int ARGV_FILTER_EXPR     = 5;
    int ARGV_FILTER_ARGS     = 6;
#define SHIFT_ARGS(i) \
    ARGV_EDGE_FILTER_TXT += i; \
    ARGV_EDGE_FILTER_VAL += i; \
    ARGV_INDEX_TXT += i; \
    ARGV_INDEX_VAL += i; \
    ARGV_ORDER_TXT += i; \
    ARGV_ORDER_FLD += i; \
    ARGV_ORDER_ORD += i; \
    ARGV_OFFSET_TXT += i; \
    ARGV_OFFSET_NUM += i; \
    ARGV_LIMIT_TXT += i; \
    ARGV_LIMIT_NUM += i; \
    ARGV_MERGE_TXT += i; \
    ARGV_MERGE_VAL += i; \
    ARGV_FIELDS_TXT += i; \
    ARGV_FIELDS_VAL += i; \
    ARGV_NODE_IDS += i; \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    if (argc < 5) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *lang = argv[ARGV_LANG];
    SVECTOR_AUTOFREE(traverse_result); /*!< for postprocessing the result. */
    __auto_free_rpn_ctx struct rpn_ctx *traversal_rpn_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *traversal_expression = NULL;
    __auto_free_rpn_ctx struct rpn_ctx *edge_filter_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *edge_filter = NULL;
    __selva_autofree RedisModuleStringList index_hints = NULL;
    int nr_index_hints = 0;

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Parse the traversal arguments.
     */
    enum SelvaTraversal dir;
    const RedisModuleString *ref_field = NULL;
    err = SelvaTraversal_ParseDir2(&dir, argv[ARGV_DIRECTION]);
    if (err) {
        replyWithSelvaErrorf(ctx, err, "Traversal argument");
        return REDISMODULE_OK;
    }
    if (argc <= ARGV_REF_FIELD &&
        (dir & (SELVA_HIERARCHY_TRAVERSAL_ARRAY |
                SELVA_HIERARCHY_TRAVERSAL_REF |
                SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD |
                SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                SELVA_HIERARCHY_TRAVERSAL_EXPRESSION))) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_OK;
    }
    if (dir & (SELVA_HIERARCHY_TRAVERSAL_ARRAY |
               SELVA_HIERARCHY_TRAVERSAL_REF |
               SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
               SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD)) {
        ref_field = argv[ARGV_REF_FIELD];
        SHIFT_ARGS(1);
    } else if (dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                      SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) {
        const RedisModuleString *input = argv[ARGV_REF_FIELD];
        TO_STR(input);

        traversal_rpn_ctx = rpn_init(1);
        traversal_expression = rpn_compile(input_str);
        if (!traversal_expression) {
            return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the traversal expression");
        }
        SHIFT_ARGS(1);
    }

    if (argc > ARGV_EDGE_FILTER_VAL) {
        const char *expr_str;

        err = SelvaArgParser_StrOpt(&expr_str, "edge_filter", argv[ARGV_EDGE_FILTER_TXT], argv[ARGV_EDGE_FILTER_VAL]);
        if (err == 0) {
            SHIFT_ARGS(2);

            if (!(dir & (SELVA_HIERARCHY_TRAVERSAL_EXPRESSION |
                         SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION))) {
                return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "edge_filter can be only used with expression traversals");
            }

            edge_filter_ctx = rpn_init(1);
            edge_filter = rpn_compile(expr_str);
            if (!edge_filter) {
                return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "edge_filter");
            }
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "edge_filter");
        }
    }

    /*
     * Parse the indexing hint.
     */
    nr_index_hints = SelvaArgParser_IndexHints(&index_hints, argv + ARGV_INDEX_TXT, argc - ARGV_INDEX_TXT);
    if (nr_index_hints < 0) {
        return replyWithSelvaError(ctx, nr_index_hints);
    } else if (nr_index_hints > 0) {
        SHIFT_ARGS(2 * nr_index_hints);
    }

    /*
     * Parse the order arg.
     */
    enum SelvaResultOrder order = SELVA_RESULT_ORDER_NONE;
    RedisModuleString *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = SelvaTraversal_ParseOrderArg(&order_by_field, &order,
                          argv[ARGV_ORDER_TXT],
                          argv[ARGV_ORDER_FLD],
                          argv[ARGV_ORDER_ORD]);
        if (err == 0) {
            SHIFT_ARGS(3);
        } else if (err != SELVA_HIERARCHY_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "order");
        }
    }

    /*
     * Parse the offset arg.
     */
    ssize_t offset = 0;
    if (argc > ARGV_OFFSET_NUM) {
        err = SelvaArgParser_IntOpt(&offset, "offset", argv[ARGV_OFFSET_TXT], argv[ARGV_OFFSET_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "offset");
        }
        if (offset < -1) {
            return replyWithSelvaErrorf(ctx, err, "offset < -1");
        }
    }

    /*
     * Parse the limit arg. -1 = inf
     */
    ssize_t limit = -1;
    if (argc > ARGV_LIMIT_NUM) {
        err = SelvaArgParser_IntOpt(&limit, "limit", argv[ARGV_LIMIT_TXT], argv[ARGV_LIMIT_NUM]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "limit");
        }
    }

    /*
     * Parse the merge flag.
     */
    enum SelvaMergeStrategy merge_strategy = MERGE_STRATEGY_NONE;
    RedisModuleString *merge_path = NULL;
    if (argc > ARGV_MERGE_VAL) {
        err = SelvaArgParser_Enum(merge_types, argv[ARGV_MERGE_TXT]);
        if (err != SELVA_ENOENT) {
            if (err < 0) {
                return replyWithSelvaErrorf(ctx, err, "invalid merge argument");
            }

            if (limit != -1) {
                return replyWithSelvaErrorf(ctx, err, "merge is not supported with limit");
            }

            merge_strategy = err;
            merge_path = argv[ARGV_MERGE_VAL];
            SHIFT_ARGS(2);
        }
    }

    /*
     * Parse fields.
     */
    selvaobject_autofree struct SelvaObject *fields = NULL;
    RedisModuleString *excluded_fields = NULL;
    __auto_free_rpn_ctx struct rpn_ctx *fields_rpn_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *fields_expression = NULL;
    __auto_free_rpn_expression struct rpn_expression *inherit_expression = NULL;
    if (argc > ARGV_FIELDS_VAL) {
        const char *argv_fields_txt = RedisModule_StringPtrLen(argv[ARGV_FIELDS_TXT], NULL);

        if (!strcmp("fields", argv_fields_txt)) {
            if (merge_strategy == MERGE_STRATEGY_ALL) {
                /* Having fields set turns a regular merge into a named merge. */
                merge_strategy = MERGE_STRATEGY_NAMED;
            } else if (merge_strategy != MERGE_STRATEGY_NONE) {
                return replyWithSelvaErrorf(ctx, err, "Only the regular merge can be used with fields");
            }

            err = SelvaArgsParser_StringSetList(ctx, &fields, &excluded_fields, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
            if (err) {
                return replyWithSelvaErrorf(ctx, err, "Parsing fields argument failed");
            }

            SHIFT_ARGS(2);
        } else if (!strcmp("fields_rpn", argv_fields_txt)) {
            const char *expr_str;

            /*
             * Note that fields_rpn and merge can't work together because the
             * field names can't vary in a merge.
             */
            if (merge_strategy != MERGE_STRATEGY_NONE) {
                return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "fields_rpn with merge not supported");
            }

            err = SelvaArgParser_StrOpt(&expr_str, "fields_rpn", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
            if (err) {
                return replyWithSelvaErrorf(ctx, err, "Parsing fields argument failed");
            }

            fields_rpn_ctx = rpn_init(1);
            fields_expression = rpn_compile(expr_str);
            if (!fields_expression) {
                return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "fields_rpn");
            }

            SHIFT_ARGS(2);
        } else if (!strcmp("inherit_rpn", argv_fields_txt)) {
            const char *expr_str;

            if (merge_strategy != MERGE_STRATEGY_NONE) {
                return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "inherit with merge not supported");
            }

            /* TODO arg parser isn't technically needed here */
            err = SelvaArgParser_StrOpt(&expr_str, "inherit_rpn", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
            if (err) {
                return replyWithSelvaErrorf(ctx, err, "Parsing inherit_rpn argument failed");
            }

            fields_rpn_ctx = rpn_init(1);
            inherit_expression = rpn_compile(expr_str);
            if (!inherit_expression) {
                return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "inherit_rpn");
            }

            SHIFT_ARGS(2);
        }
    }
    if (merge_strategy != MERGE_STRATEGY_NONE && (!fields || SelvaTraversal_FieldsContains(fields, "*", 1))) {
        if (fields) {
            SelvaObject_Destroy(fields);
        }

        /* Merge needs a fields object anyway but it must be empty. */
        fields = SelvaObject_New();
    }

    /*
     * Prepare the filter expression if given.
     */
    RedisModuleString *argv_filter_expr = NULL;
    __auto_free_rpn_ctx struct rpn_ctx *rpn_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *filter_expression = NULL;
    if (argc >= ARGV_FILTER_EXPR + 1) {
        argv_filter_expr = argv[ARGV_FILTER_EXPR];
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;

        rpn_ctx = rpn_init(nr_reg);
        filter_expression = rpn_compile(RedisModule_StringPtrLen(argv_filter_expr, NULL));
        if (!filter_expression) {
            return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the filter expression");
        }

        /*
         * Get the filter expression arguments and set them to the registers.
         */
        for (int i = ARGV_FILTER_ARGS; i < argc; i++) {
            /* reg[0] is reserved for the current nodeId */
            const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
            size_t str_len;
            const char *str = RedisModule_StringPtrLen(argv[i], &str_len);

            rpn_set_reg(rpn_ctx, reg_i, str, str_len + 1, 0);
        }
    }

    if (argc <= ARGV_NODE_IDS) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_EINVAL);
    }

    const RedisModuleString *ids = argv[ARGV_NODE_IDS];
    TO_STR(ids);

    if (inherit_expression) {
        SVector_Init(&traverse_result, HIERARCHY_EXPECTED_RESP_LEN, NULL);
    } else if (order != SELVA_RESULT_ORDER_NONE) {
        SelvaTraversalOrder_InitOrderResult(&traverse_result, order, limit);
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    if (nr_index_hints > 0) {
        /*
         * Limit and indexing can be only used together when an order is requested
         * to guarantee a deterministic response order.
         */
        if (limit != -1 && order == SELVA_RESULT_ORDER_NONE) {
            nr_index_hints = 0;
        }
    }

    /*
     * Run for each NODE_ID.
     */
    ssize_t nr_nodes = 0;
    size_t merge_nr_fields = 0;
    SelvaFind_Postprocess postprocess = NULL;
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        Selva_NodeId nodeId;

        Selva_NodeIdCpy(nodeId, ids_str + i);
        if (nodeId[0] == '\0') {
            /* Just skip empty IDs. */
            continue;
        }

        /*
         * Note that SelvaArgParser_IndexHints() limits the nr_index_hints to
         * FIND_INDICES_MAX_HINTS_FIND
         */
        struct SelvaFindIndexControlBlock *ind_icb[max(nr_index_hints, 1)];
        int ind_select = -1; /* Selected index. The smallest of all found. */

        memset(ind_icb, 0, max(nr_index_hints, 1) * sizeof(struct SelvaFindIndexControlBlock *));

        /* find_indices_max == 0 => indexing disabled */
        if (nr_index_hints > 0 && selva_glob_config.find_indices_max > 0) {
            RedisModuleString *dir_expr = NULL;

            if (dir & (SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                       SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) {
                /*
                 * We know it's valid because it was already parsed and compiled once.
                 * However, the indexing subsystem can't use the already compiled
                 * expression because its lifetime is unpredictable and it's not easy
                 * to change that.
                 */
                dir_expr = argv[ARGV_REF_FIELD];
            }

            /*
             * Select the best index res set.
             */
            ind_select = SelvaFindIndex_AutoMulti(ctx, hierarchy, dir, dir_expr, nodeId, order, order_by_field, index_hints, nr_index_hints, ind_icb);
        }

        /*
         * If the index is already ordered then we don't need to sort the
         * response. This won't work if we have multiple nodeIds because
         * obviously the order might differ and we may not have an ordered
         * index for each id.
         */
        if (ind_select >= 0 &&
            ids_len == SELVA_NODE_ID_SIZE &&
            SelvaFindIndex_IsOrdered(ind_icb[ind_select], order, order_by_field)) {
            order = SELVA_RESULT_ORDER_NONE;
            order_by_field = NULL; /* This controls sorting in the callback. */
        }

        /*
         * Run BFS/DFS.
         */
        ssize_t tmp_limit = -1;
        const size_t skip = ind_select >= 0 ? 0 : SelvaTraversal_GetSkip(dir); /* Skip n nodes from the results. */
        struct FindCommand_Args args = {
            .lang = lang,
            .nr_nodes = &nr_nodes,
            .offset = (order == SELVA_RESULT_ORDER_NONE) ? offset + skip : skip,
            .limit = (order == SELVA_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
            .send_param.merge_strategy = merge_strategy,
            .send_param.merge_path = merge_path,
            .merge_nr_fields = &merge_nr_fields,
            .send_param.fields = fields,
            .send_param.fields_rpn_ctx = fields_rpn_ctx,
            .send_param.fields_expression = fields_expression,
            .send_param.inherit_expression = inherit_expression,
            .send_param.excluded_fields = excluded_fields,
            .send_param.order = order,
            .send_param.order_field = order_by_field,
            .result = &traverse_result,
            .acc_tot = 0,
            .acc_take = 0,
        };

        if (limit == 0) {
            break;
        }

        if (dir == SELVA_HIERARCHY_TRAVERSAL_ARRAY) {
            if (order != SELVA_RESULT_ORDER_NONE) {
                args.process_obj = &process_array_obj_sort;
                postprocess = &postprocess_array;
            } else {
                args.process_obj = &process_array_obj_send;
                postprocess = NULL;
            }
        } else {
            if (inherit_expression) {
                /* This will also handle sorting if it was requested. */
                args.process_node = &process_node_inherit;
                postprocess = &postprocess_inherit;
            } else if (order != SELVA_RESULT_ORDER_NONE) {
                args.process_node = &process_node_sort;
                postprocess = &postprocess_sort;
            } else {
                args.process_node = &process_node_send;
                postprocess = NULL;
            }
        }

        if (ind_select >= 0) {
            /*
             * There is no need to run the filter again if the indexing was
             * executing the same filter already.
             */
            if (argv_filter_expr && !RedisModule_StringCompare(argv_filter_expr, index_hints[ind_select])) {
                args.rpn_ctx = NULL;
                args.filter = NULL;
            }

            SELVA_TRACE_BEGIN(cmd_find_index);
            err = SelvaFindIndex_Traverse(ctx, hierarchy, ind_icb[ind_select], FindCommand_NodeCb, &args);
            SELVA_TRACE_END(cmd_find_index);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_ARRAY && ref_field) {
            struct FindCommand_ArrayObjectCb array_args = {
                .ctx = ctx,
                .find_args = &args,
            };
            const struct SelvaObjectArrayForeachCallback ary_cb = {
                .cb = FindCommand_ArrayObjectCb,
                .cb_arg = &array_args,
            };
            TO_STR(ref_field);

            SELVA_TRACE_BEGIN(cmd_find_array);
            err = SelvaHierarchy_TraverseArray(ctx, hierarchy, nodeId, ref_field_str, ref_field_len, &ary_cb);
            SELVA_TRACE_END(cmd_find_array);
        } else if ((dir & (SELVA_HIERARCHY_TRAVERSAL_REF |
                    SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                    SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD))
                   && ref_field) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = FindCommand_NodeCb,
                .node_arg = &args,
            };
            TO_STR(ref_field);

            SELVA_TRACE_BEGIN(cmd_find_refs);
            err = SelvaHierarchy_TraverseField(ctx, hierarchy, nodeId, dir, ref_field_str, ref_field_len, &cb);
            SELVA_TRACE_END(cmd_find_refs);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = FindCommand_NodeCb,
                .node_arg = &args,
            };

            SELVA_TRACE_BEGIN(cmd_find_bfs_expression);
            err = SelvaHierarchy_TraverseExpressionBfs(ctx, hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, edge_filter_ctx, edge_filter, &cb);
            SELVA_TRACE_END(cmd_find_bfs_expression);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_EXPRESSION) {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = FindCommand_NodeCb,
                .node_arg = &args,
            };

            SELVA_TRACE_BEGIN(cmd_find_traversal_expression);
            err = SelvaHierarchy_TraverseExpression(ctx, hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, edge_filter_ctx, edge_filter, &cb);
            SELVA_TRACE_END(cmd_find_traversal_expression);
        } else {
            const struct SelvaHierarchyCallback cb = {
                .node_cb = FindCommand_NodeCb,
                .node_arg = &args,
            };

            SELVA_TRACE_BEGIN(cmd_find_rest);
            err = SelvaHierarchy_Traverse(ctx, hierarchy, nodeId, dir, &cb);
            SELVA_TRACE_END(cmd_find_rest);
        }
        if (err != 0) {
            /*
             * We can't send an error to the client at this point so we'll just log
             * it and ignore the error.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "Find failed. err: %s dir: %s node_id: \"%.*s\"\n",
                      getSelvaErrorStr(err),
                      SelvaTraversal_Dir2str(dir),
                      (int)SELVA_NODE_ID_SIZE, nodeId);
        }

        /*
         * Do index accounting.
         */
        SelvaFindIndex_AccMulti(ind_icb, nr_index_hints, ind_select, args.acc_take, args.acc_tot);
    }

    if (postprocess) {
        struct SelvaNodeSendParam send_args = {
            .order = order,
            .order_field = order_by_field,
            .merge_strategy = merge_strategy,
            .merge_path = merge_path,
            .fields = fields,
            .fields_rpn_ctx = fields_rpn_ctx,
            .fields_expression = fields_expression,
            .inherit_expression = inherit_expression,
            .excluded_fields = excluded_fields,
        };

        SELVA_TRACE_BEGIN(cmd_find_sort_result);
        postprocess(ctx, hierarchy, lang, offset, limit, &send_args, &traverse_result);
        SELVA_TRACE_END(cmd_find_sort_result);
    } else {
        RedisModule_ReplySetArrayLength(ctx, get_nr_out(merge_strategy, nr_nodes, merge_nr_fields));
    }

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

static int Find_OnLoad(RedisModuleCtx *ctx) {
    return RedisModule_CreateCommand(ctx, "selva.hierarchy.find", SelvaHierarchy_FindCommand, "readonly", 2, 2, 1);
}
SELVA_ONLOAD(Find_OnLoad);
