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
#include "auto_free.h"
#include "arg_parser.h"
#include "errors.h"
#include "hierarchy.h"
#include "rpn.h"
#include "selva.h"
#include "config.h"
#include "selva_lang.h"
#include "selva_node.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "selva_trace.h"
#include "subscriptions.h"
#include "edge.h"
#include "svector.h"
#include "cstrings.h"
#include "traversal.h"
#include "find_index.h"

static int send_node_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *field,
        RedisModuleString *excluded_fields);
static int send_all_node_data_fields(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *excluded_fields);

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

static int send_hierarchy_field(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId nodeId,
        RedisModuleString *full_field_name,
        const char *field_str,
        size_t field_len) {
#define SEND_FIELD_NAME() RedisModule_ReplyWithString(ctx, full_field_name)

    /*
     * Check if the field name is a hierarchy field name.
     * We use length check and memcmp() here instead of strcmp() because it
     * seems to give us a much better branch prediction success rate and
     * this function is pretty hot.
     */
    if (field_len == 9 && !memcmp(field_str, "ancestors", 9)) {
        SEND_FIELD_NAME();
        return HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS);
    } else if (field_len == 8 && !memcmp(field_str, "children", 8)) {
        SEND_FIELD_NAME();
        return HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_CHILDREN);
    } else if (field_len == 11 && !memcmp(field_str, "descendants", 11)) {
        SEND_FIELD_NAME();
        return HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS);
    } else if (field_len == 7 && !memcmp(field_str, "parents", 7)) {
        SEND_FIELD_NAME();
        return HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_PARENTS);
    }

    return SELVA_ENOENT;
#undef SEND_FIELD_NAME
}

static int send_edge_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaObject *edges,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *field,
        RedisModuleString *excluded_fields) {
    struct EdgeField *edge_field;
    void *p;
    TO_STR(field);

    int off = SelvaObject_GetPointerPartialMatchStr(edges, field_str, field_len, &p);
    edge_field = p;
    if (off < 0) {
        return off;
    } else if (!edge_field) {
        return SELVA_ENOENT;
    } else if (off == 0) {
        RedisModuleString *act_field_name;

        if (field_prefix_str) {
            act_field_name = RedisModule_CreateStringPrintf(ctx, "%.*s%s", (int)field_prefix_len, field_prefix_str, field_str);
            if (!act_field_name) {
                return SELVA_ENOMEM;
            }
        } else {
            act_field_name = field;
        }

        RedisModule_ReplyWithString(ctx, act_field_name);
        replyWithEdgeField(ctx, edge_field);
        return 0;
    }

    if (!edge_field->constraint || !(edge_field->constraint->flags & EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF)) {
        return SELVA_EINTYPE;
    }

    struct SelvaHierarchyNode *dst_node;
    dst_node = SVector_GetIndex(&edge_field->arcs, 0);
    if (!dst_node) {
        Selva_NodeId node_id;

        SelvaHierarchy_GetNodeId(node_id, node);
        fprintf(stderr, "%s:%d: Edge %.*s.%.*s shouldn't contain NULL arcs\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, node_id,
                off, field_str);
        return SELVA_ENOENT;
    }

    /*
     * Note: The dst_node might be the same as node but this shouldn't case
     * an infinite loop or any other issues as we'll be always cutting the
     * field name shorter and thus the recursion should eventually stop.
     */
    struct SelvaObject *dst_obj;
    dst_obj = SelvaHierarchy_GetNodeObject(dst_node);

    const char *next_field_str = field_str + off;
    size_t next_field_len = field_len - off;
    RedisModuleString *next_field = RedisModule_CreateString(ctx, next_field_str, next_field_len);
    if (!next_field) {
        return SELVA_ENOMEM;
    }

    const char *next_prefix_str;
    size_t next_prefix_len;

    if (field_prefix_str) {
        const char *s = strnstr(field_str, ".", field_len);
        const int n = s ? (int)(s - field_str) + 1 : (int)field_len;
        const RedisModuleString *next_prefix;

        next_prefix = RedisModule_CreateStringPrintf(ctx, "%.*s%.*s", (int)field_prefix_len, field_prefix_str, n, field_str);
        next_prefix_str = RedisModule_StringPtrLen(next_prefix, &next_prefix_len);
    } else {
        next_prefix_str = field_str;
        next_prefix_len = off;
    }

    /* TODO could close the key */
    if (next_field_len == 1 && next_field_str[0] == '*') {
        int res;

        RedisModule_ReplyWithStringBuffer(ctx, next_prefix_str, next_prefix_len - 1);
        RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

        res = send_all_node_data_fields(ctx, lang, hierarchy, dst_node, dst_obj, NULL, 0, excluded_fields);
        if (res < 0) {
            res = 0;
        }

        RedisModule_ReplySetArrayLength(ctx, 2 * res);
        return 0;
    } else {
        return (send_node_field(ctx, lang, hierarchy, dst_node, dst_obj, next_prefix_str, next_prefix_len, next_field, excluded_fields) == 0) ? SELVA_ENOENT : 0;
    }
}

static int is_excluded_field(RedisModuleString *excluded_fields, RedisModuleString *field_name) {
    TO_STR(excluded_fields, field_name);
    const char *r;

    /* RMS always ends with '\0', so let's utilize that. */
    r = memmem(excluded_fields_str, excluded_fields_len, field_name_str, field_name_len + 1);

    /* The list is made in a way that each element is delimited by '\0' from beginning and end. */
    return r && *(r - 1) == '\0';
}

static int send_node_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *field,
        RedisModuleString *excluded_fields) {
    TO_STR(field);
    Selva_NodeId nodeId;
    int err;

    SelvaHierarchy_GetNodeId(nodeId, node);

    RedisModuleString *full_field_name;
    if (field_prefix_str) {
        full_field_name = RedisModule_CreateStringPrintf(ctx, "%.*s%s", (int)field_prefix_len, field_prefix_str, field_str);
        if (!full_field_name) {
            return SELVA_ENOMEM;
        }
    } else {
        full_field_name = field;
    }

    if (excluded_fields && is_excluded_field(excluded_fields, full_field_name)) {
        /*
         * This field should be excluded from the results.
         */
        return 0;
    }

    /*
     * Check if the field name is a hierarchy field name.
     */
    err = send_hierarchy_field(ctx, hierarchy, nodeId, full_field_name, field_str, field_len);
    if (err == 0) {
        return 1;
    } else if (err != SELVA_ENOENT) {
        fprintf(stderr, "%s:%d: Sending the %s field of %.*s failed: %s\n",
                __FILE__, __LINE__,
                field_str,
                (int)SELVA_NODE_ID_SIZE, nodeId,
                getSelvaErrorStr(err));
        return 1; /* Something was already sent so +1 */
    } else {
        /*
         * Check if the field name is an edge field.
         */
        struct SelvaHierarchyMetadata *metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);
        struct SelvaObject *edges = metadata->edge_fields.edges;

        if (edges) {
            err = send_edge_field(ctx, lang, hierarchy, node, edges, field_prefix_str, field_prefix_len, field, excluded_fields);
            if (err == 0) {
               return 1;
            } else if (err != SELVA_ENOENT) {
                return 0;
            }
        }
    }

    /*
     * Check if we have a wildcard in the middle of the field name
     * and process it.
     */
    if (strstr(field_str, ".*.")) {
        long resp_count = 0;
        err = SelvaObject_GetWithWildcardStr(ctx, lang, obj, field_str, field_len, &resp_count, -1, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err && err != SELVA_ENOENT) {
            fprintf(stderr, "%s:%d: Sending wildcard field %.*s of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)field_len, field_str,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    getSelvaErrorStr(err));
        }

        return (int)(resp_count / 2);
    }

    /*
     * Finally check if the field name is a key on the node object.
     */

    if (field_len >= 2 && field_str[field_len - 2] == '.' && field_str[field_len - 1] == '*') {
        field_len -= 2;
    } else if (SelvaObject_Exists(obj, field)) {
        /* Field didn't exist in the node. */
        return 0;
    }

    /*
     * Send the reply.
     */
    RedisModule_ReplyWithString(ctx, RedisModule_CreateStringPrintf(ctx, "%.*s%.*s", (int)field_prefix_len, field_prefix_str, (int)field_len, field_str));
    err = SelvaObject_ReplyWithObjectStr(ctx, lang, obj, field_str, field_len, SELVA_OBJECT_REPLY_BINUMF_FLAG);
    if (err) {
        fprintf(stderr, "%s:%d: Failed to send the field (%.*s) for node_id: \"%.*s\" err: \"%s\"\n",
                __FILE__, __LINE__,
                (int)field_len, field_str,
                (int)SELVA_NODE_ID_SIZE, nodeId,
                getSelvaErrorStr(err));
        RedisModule_ReplyWithNull(ctx);
    }

    return 1;
}

static int send_all_node_data_fields(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *excluded_fields) {
    void *iterator;
    const char *field_name_str;
    int nr_fields = 0;

    iterator = SelvaObject_ForeachBegin(obj);
    while ((field_name_str = SelvaObject_ForeachKey(obj, &iterator))) {
        size_t field_name_len = strlen(field_name_str);
        RedisModuleString *field;
        int res;

        field = RedisModule_CreateString(ctx, field_name_str, field_name_len);
        if (!field) {
            /*
             * Resist the devil, and he will flee from you.
             * We are probably going to crash soon and there is no way out at
             * this point, so let's just ignore this ever happened.
             */
            continue;
        }

        res = send_node_field(ctx, lang, hierarchy, node, obj, field_prefix_str, field_prefix_len, field, excluded_fields);
        if (res >= 0) {
            nr_fields += res;
        } else {
            /* RFE errors are ignored for now. */
            Selva_NodeId node_id;

            SelvaHierarchy_GetNodeId(node_id, node);
            fprintf(stderr, "%s:%d: send_node_field(%.*s) failed: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node_id,
                    getSelvaErrorStr(res));
        }
    }

    return nr_fields;
}

static int send_node_fields(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaObject *fields,
        RedisModuleString *excluded_fields) {
    const char wildcard[] = "*";
    Selva_NodeId nodeId;
    struct SelvaObject *obj;
    int err;

    SelvaHierarchy_GetNodeId(nodeId, node);
    obj = SelvaHierarchy_GetNodeObject(node);

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
    } else if (!excluded_fields && fields_len == 1 &&
               SelvaTraversal_FieldsContains(fields, wildcard, sizeof(wildcard) - 1)) {
        err = SelvaObject_ReplyWithObject(ctx, lang, obj, NULL, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to send all fields for node_id: \"%.*s\"\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId);
        }
    } else {
        _Static_assert(sizeof(wildcard) == 2, "Must be a single char");
        const char wildcard_ch = wildcard[0];
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
                TO_STR(field);
                int res;

                if (field_len == 1 && field_str[0] == wildcard_ch) {
                    res = send_all_node_data_fields(ctx, lang, hierarchy, node, obj, NULL, 0, excluded_fields);
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
                    res = send_node_field(ctx, lang, hierarchy, node, obj, NULL, 0, field, excluded_fields);
                    if (res > 0) {
                        nr_fields += res;
                        break; /* Only send one of the fields in the list. */
                    }
                }
            }
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
        if (!full_field_name) {
            return SELVA_ENOMEM;
        }
    } else {
        full_field_name = field;
    }

    /*
     * Check if we have a wildcard in the middle of the field name
     * and process it.
     */
    if (strstr(field_str, ".*.")) {
        long resp_count = 0;

        err = SelvaObject_GetWithWildcardStr(ctx, lang, obj, field_str, field_len, &resp_count, -1, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err && err != SELVA_ENOENT) {
            fprintf(stderr, "%s:%d: Sending wildcard field %.*s in array object failed: %s\n",
                    __FILE__, __LINE__,
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
        fprintf(stderr, "%s:%d: Failed to send the field (%s) in array object err: \"%s\"\n",
                __FILE__, __LINE__,
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
    } else if (fields_len == 1 && SelvaTraversal_FieldsContains(fields, "*", 1)) { /* '*' is a wildcard */
        err = SelvaObject_ReplyWithObject(ctx, lang, obj, NULL, SELVA_OBJECT_REPLY_BINUMF_FLAG);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to send all fields for selva object in array: %s\n",
                    __FILE__, __LINE__,
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

static ssize_t send_merge_all(
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
        int err;

        if (!SelvaObject_ExistsStr(fields, key_name_str, strlen(key_name_str))) {
            continue;
        }

        ++*nr_fields_out;

        RedisModuleString *full_field_path;
        full_field_path = format_full_field_path(ctx, obj_path_str, key_name_str);
        if (!full_field_path) {
            fprintf(stderr, "%s:%d: Out of memory\n", __FILE__, __LINE__);
            replyWithSelvaError(ctx, SELVA_ENOMEM);
            continue;
        }

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

            fprintf(stderr, "%s:%d: Failed to send \"%s.%s\" of node_id: \"%.*s\"\n",
                    __FILE__, __LINE__,
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

static ssize_t send_named_merge(
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
            int err;

            if (SelvaObject_Exists(obj, field)) {
                continue;
            }

            ++*nr_fields_out;

            RedisModuleString *full_field_path;
            full_field_path = format_full_field_path(ctx, obj_path_str, RedisModule_StringPtrLen(field, NULL));
            if (!full_field_path) {
                fprintf(stderr, "%s:%d: Out of memory\n", __FILE__, __LINE__);
                replyWithSelvaError(ctx, SELVA_ENOMEM);
                continue;
            }

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

                fprintf(stderr, "%s:%d: Failed to send the field (%s) for node_id: \"%.*s\" err: \"%s\"\n",
                        __FILE__, __LINE__,
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

static ssize_t send_deep_merge(
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
        if (!next_path) {
            return SELVA_ENOMEM;
        }

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
                fprintf(stderr, "%s:%d: Deep merge failed %s\n",
                        __FILE__, __LINE__,
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

                fprintf(stderr, "%s:%d: Failed to send \"%s.%s\" of node_id: \"%.*s\"\n",
                        __FILE__, __LINE__,
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

static ssize_t send_node_object_merge(
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

    ssize_t res;
    if ((merge_strategy == MERGE_STRATEGY_ALL || merge_strategy == MERGE_STRATEGY_DEEP) &&
        is_text_field(node_obj, obj_path_str, obj_path_len)) {
        /*
         * If obj is a text field we can just send it directly and skip the rest of
         * the processing.
         */
        if (SelvaObject_GetType(fields, obj_path) != SELVA_OBJECT_LONGLONG) {
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
                TO_STR(obj_path);

                fprintf(stderr, "%s:%d: Failed to send \"%s\" (text) of node_id: \"%.*s\"\n",
                        __FILE__, __LINE__,
                        obj_path_str,
                        (int)SELVA_NODE_ID_SIZE, nodeId);
            } else {
                /* Mark the key as sent. */
                (void)SelvaObject_SetLongLong(fields, obj_path, 1);
            }
        }

        res = 0;
    } else if (merge_strategy == MERGE_STRATEGY_ALL) {
        /* Send all keys from the nested object. */
        res = send_merge_all(ctx, lang, nodeId, fields, obj, obj_path, nr_fields_out);
    } else if (merge_strategy == MERGE_STRATEGY_NAMED) {
        /* Send named keys from the nested object. */
        res = send_named_merge(ctx, lang, nodeId, fields, obj, obj_path, nr_fields_out);
    } else if (merge_strategy == MERGE_STRATEGY_DEEP) {
        /* Deep merge all keys and nested objects. */
        res = send_deep_merge(ctx, lang, nodeId, fields, obj, obj_path, nr_fields_out);
    } else {
        res = replyWithSelvaErrorf(ctx, SELVA_ENOTSUP, "Merge strategy not supported: %d\n", (int)merge_strategy);
    }

    return res;
}

static int exec_fields_expression(
        struct RedisModuleCtx *redis_ctx,
        const struct SelvaHierarchyNode *node,
        struct rpn_ctx *rpn_ctx,
        const struct rpn_expression *expr,
        struct SelvaObject *fields) {
    Selva_NodeId nodeId;
    struct SelvaSet set;
    enum rpn_error rpn_err;
    struct SelvaSetElement *el;
    size_t i;

    SelvaHierarchy_GetNodeId(nodeId, node);
    rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
    rpn_set_hierarchy_node(rpn_ctx, node);

    SelvaSet_Init(&set, SELVA_SET_TYPE_RMSTRING);
    rpn_err = rpn_selvaset(redis_ctx, rpn_ctx, expr, &set);
    if (rpn_err) {
        /*
         * TODO Not the best error to return here but the client won't see this
         * anyway.
         */
        return SELVA_EGENERAL;
    }

    i = 0;
    SELVA_SET_RMS_FOREACH(el, &set) {
        const size_t key_len = (size_t)(log10(i + 1)) + 1;
        char key_str[key_len + 1];

        snprintf(key_str, key_len + 1, "%zu", i);
        SelvaObject_AddArrayStr(fields, key_str, key_len, SELVA_OBJECT_STRING, RedisModule_HoldString(NULL, el->value_rms));
        i++;
    }

    SelvaSet_Destroy(&set);

    return 0;
}

static __hot int FindCommand_NodeCb(struct SelvaHierarchyNode *node, void *arg) {
    Selva_NodeId nodeId;
    struct FindCommand_Args *args = (struct FindCommand_Args *)arg;
    struct rpn_ctx *rpn_ctx = args->rpn_ctx;
    int take = (args->offset > 0) ? !args->offset-- : 1;

    SelvaHierarchy_GetNodeId(nodeId, node);

    args->acc_tot++;
    if (take && rpn_ctx) {
        int err;

        /* Set node_id to the register */
        rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
        rpn_set_hierarchy_node(rpn_ctx, node);

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(args->ctx, rpn_ctx, args->filter, &take);
        if (err) {
            fprintf(stderr, "%s:%d: Expression failed (node: \"%.*s\"): \"%s\"\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    rpn_str_error[err]);
            return 1;
        }
    }

    if (take) {
        const int sort = !!args->order_field;

        args->acc_take++;

        if (!sort) {
            ssize_t *nr_nodes = args->nr_nodes;
            ssize_t * restrict limit = args->limit;
            int err;

            if (args->merge_strategy != MERGE_STRATEGY_NONE) {
                err = send_node_object_merge(args->ctx, args->lang, node, args->merge_strategy, args->merge_path, args->fields, args->merge_nr_fields);
            } else if (args->fields) {
                err = send_node_fields(args->ctx, args->lang, args->hierarchy, node, args->fields, args->excluded_fields);
            } else if (args->fields_expression) {
                selvaobject_autofree struct SelvaObject *fields = SelvaObject_New();

                err = exec_fields_expression(args->ctx, node, args->fields_rpn_ctx, args->fields_expression, fields);
                if (!err) {
                    err = send_node_fields(args->ctx, args->lang, args->hierarchy, node, fields, args->excluded_fields);
                }
            } else {
                RedisModule_ReplyWithStringBuffer(args->ctx, nodeId, Selva_NodeIdLen(nodeId));
                err = 0;
            }
            if (err) {
                RedisModule_ReplyWithNull(args->ctx);
                fprintf(stderr, "%s:%d: Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, nodeId,
                        getSelvaErrorStr(err));
            }

            *nr_nodes = *nr_nodes + 1;

            *limit = *limit - 1;
            if (*limit == 0) {
                return 1;
            }
        } else {
            struct TraversalOrderedItem *item;

            item = SelvaTraversal_CreateOrderItem(args->ctx, args->lang, node, args->order_field);
            if (item) {
                SVector_InsertFast(args->order_result, item);
            } else {
                /*
                 * It's not so easy to make the response fail at this point.
                 * Given that we shouldn't generally even end up here in real
                 * life, it's fairly ok to just log the error and return what
                 * we can.
                 */
                fprintf(stderr, "%s:%d: Out of memory while creating an ordered result item\n",
                        __FILE__, __LINE__);
            }
        }
    }

    return 0;
}

static int FindCommand_ArrayNodeCb(struct SelvaObject *obj, void *arg) {
    struct FindCommand_Args *args = (struct FindCommand_Args *)arg;
    struct rpn_ctx *rpn_ctx = args->rpn_ctx;
    int take = (args->offset > 0) ? !args->offset-- : 1;

    if (take && rpn_ctx) {
        int err;

        /* Set obj to the register */
        err = rpn_set_reg_slvobj(rpn_ctx, 0, obj, 0);
        if (err) {
            fprintf(stderr, "%s:%d: Register set failed: \"%s\"\n",
                    __FILE__, __LINE__,
                    rpn_str_error[err]);
            return 1;
        }
        rpn_set_obj(rpn_ctx, obj);

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(args->ctx, rpn_ctx, args->filter, &take);
        if (err) {
            fprintf(stderr, "%s:%d: Expression failed: \"%s\"\n",
                    __FILE__, __LINE__,
                    rpn_str_error[err]);
            return 1;
        }
    }

    if (take) {
        const int sort = !!args->order_field;

        if (!sort) {
            ssize_t *nr_nodes = args->nr_nodes;
            ssize_t * restrict limit = args->limit;
            int err;

            if (args->fields) {
                err = send_array_object_fields(args->ctx, args->lang, obj, args->fields);
            } else {
                RedisModule_ReplyWithStringBuffer(args->ctx, EMPTY_NODE_ID, SELVA_NODE_ID_SIZE);
                err = 0;
            }
            if (err) {
                RedisModule_ReplyWithNull(args->ctx);
                fprintf(stderr, "%s:%d: Failed to handle field(s), err: %s\n",
                        __FILE__, __LINE__,
                        getSelvaErrorStr(err));
            }

            *nr_nodes = *nr_nodes + 1;

            *limit = *limit - 1;
            if (*limit == 0) {
                return 1;
            }
        } else {
            struct TraversalOrderedItem *item;
            item = SelvaTraversal_CreateObjectBasedOrderItem(args->ctx, args->lang, obj, args->order_field);
            if (item) {
                SVector_InsertFast(args->order_result, item);
            } else {
                /*
                 * It's not so easy to make the response fail at this point.
                 * Given that we shouldn't generally even end up here in real
                 * life, it's fairly ok to just log the error and return what
                 * we can.
                 */
                fprintf(stderr, "%s:%d: Out of memory while creating an ordered result item\n",
                        __FILE__, __LINE__);
            }
        }
    }

    return 0;
}

/**
 * @param nr_fields_out Only set when merge_strategy != MERGE_STRATEGY_NONE.
 */
static size_t FindCommand_PrintOrderedResult(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaHierarchy *hierarchy,
        ssize_t offset,
        ssize_t limit,
        enum SelvaMergeStrategy merge_strategy,
        RedisModuleString *merge_path,
        struct SelvaObject *fields,
        RedisModuleString *excluded_fields,
        SVector *order_result,
        size_t *nr_fields_out) {
    struct TraversalOrderedItem *item;
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
        int err;
        if (limit-- == 0) {
            break;
        }

        if (merge_strategy != MERGE_STRATEGY_NONE) {
            err = send_node_object_merge(ctx, lang, item->node, merge_strategy, merge_path, fields, nr_fields_out);
        } else if (fields) {
            struct SelvaHierarchyNode *node = item->node;

            if (node) {
                err = send_node_fields(ctx, lang, hierarchy, node, fields, excluded_fields);
            } else {
                err = SELVA_HIERARCHY_ENOENT;
            }
        } else {
            RedisModule_ReplyWithStringBuffer(ctx, item->node_id, Selva_NodeIdLen(item->node_id));
            err = 0;
        }
        if (err) {
            RedisModule_ReplyWithNull(ctx);
            fprintf(stderr, "%s:%d: Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, item->node_id,
                    getSelvaErrorStr(err));
        }

        len++;
    }

    return len;
}

static size_t FindCommand_PrintOrderedArrayResult(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        ssize_t offset,
        ssize_t limit,
        struct SelvaObject *fields,
        SVector *order_result) {
    struct TraversalOrderedItem *item;
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
        int err;
        if (limit-- == 0) {
            break;
        }

        if (item->data_obj) {
            err = send_array_object_fields(ctx, lang, item->data_obj, fields);
        } else {
            err = SELVA_HIERARCHY_ENOENT;
        }

        if (err) {
            RedisModule_ReplyWithNull(ctx);
            fprintf(stderr, "%s:%d: Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, item->node_id,
                    getSelvaErrorStr(err));
        }

        len++;
    }

    return len;
}

/**
 * Find node(s) matching the query.
 * SELVA.HIERARCHY.find lang REDIS_KEY dir [field_name/expr] [index [expr]] [order field asc|desc] [offset 1234] [limit 1234] [merge path] [fields field_names] NODE_IDS [expression] [args...]
 *                                     |   |                 |              |                      |             |            |            |                    |        |            |
 * Traversal method/direction --------/    |                 |              |                      |             |            |            |                    |        |            |
 * Traversed field or expression ---------/                  |              |                      |             |            |            |                    |        |            |
 * Indexing hint -------------------------------------------/               |                      |             |            |            |                    |        |            |
 * Sort order of the results ----------------------------------------------/                       |             |            |            |                    |        |            |
 * Skip the first 1234 - 1 results ---------------------------------------------------------------/              |            |            |                    |        |            |
 * Limit the number of results (Optional) ----------------------------------------------------------------------/             |            |                    |        |            |
 * Merge fields. fields option must be set. ---------------------------------------------------------------------------------/             |                    |        |            |
 * Return field values instead of node names ---------------------------------------------------------------------------------------------/                     |        |            |
 * One or more node IDs concatenated (10 chars per ID) --------------------------------------------------------------------------------------------------------/         |            |
 * RPN filter expression -----------------------------------------------------------------------------------------------------------------------------------------------/             |
 * Register arguments for the RPN filter --------------------------------------------------------------------------------------------------------------------------------------------/
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
    int ARGV_INDEX_TXT       = 4;
    int ARGV_INDEX_VAL       = 5;
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
    SVECTOR_AUTOFREE(order_result); /*!< for ordered result. */
    __auto_free_rpn_ctx struct rpn_ctx *traversal_rpn_ctx = NULL;
    __auto_free_rpn_expression struct rpn_expression *traversal_expression = NULL;
    __auto_free RedisModuleString **index_hints = NULL;
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
        if (!traversal_rpn_ctx) {
            return replyWithSelvaError(ctx, SELVA_ENOMEM);
        }

        traversal_expression = rpn_compile(input_str);
        if (!traversal_expression) {
            return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the traversal expression");
        }
        SHIFT_ARGS(1);
    }

    /*
     * Parse the indexing hint.
     */
    nr_index_hints = SelvaArgParser_IndexHints(&index_hints, argv + ARGV_INDEX_TXT, argc - ARGV_INDEX_TXT);
    if (nr_index_hints < 0) {
        return replyWithSelvaError(ctx, SELVA_ENOMEM);
    } else if (nr_index_hints > 0) {
        SHIFT_ARGS(2 * nr_index_hints);
    }

    /*
     * Parse the order arg.
     */
    enum SelvaResultOrder order = HIERARCHY_RESULT_ORDER_NONE;
    const RedisModuleString *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = SelvaTraversal_ParseOrder(&order_by_field, &order,
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
    if (argc > ARGV_FIELDS_VAL) {
        err = SelvaArgsParser_StringSetList(ctx, &fields, &excluded_fields, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
        if (err == SELVA_ENOENT && merge_strategy == MERGE_STRATEGY_NONE) {
            const char *expr_str;

            err = SelvaArgParser_StrOpt(&expr_str, "fields_rpn", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
            if (err == 0) {
                fields_rpn_ctx = rpn_init(1);
                if (!fields_rpn_ctx) {
                    return replyWithSelvaErrorf(ctx, SELVA_ENOMEM, "fields_rpn");
                }

                fields_expression = rpn_compile(expr_str);
                if (!fields_expression) {
                    return replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "fields_rpn");
                }
            }
        }

        /*
         * If fields argument was found.
         */
        if (err == 0) {
            if (merge_strategy == MERGE_STRATEGY_ALL) {
                /* Having fields set turns a regular merge into a named merge. */
                merge_strategy = MERGE_STRATEGY_NAMED;
            } else if (merge_strategy != MERGE_STRATEGY_NONE) {
                return replyWithSelvaErrorf(ctx, err, "Only the regular merge can be used with fields");
            }
            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "Parsing fields argument failed");
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
        if (!rpn_ctx) {
            return replyWithSelvaErrorf(ctx, SELVA_ENOMEM, "filter expression");
        }

        /*
         * Compile the filter expression.
         */
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

    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        if (!SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, SelvaTraversal_GetOrderFunc(order))) {
            return replyWithSelvaError(ctx, SELVA_ENOMEM);
        }
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * An index result set together with limit may yield different order than
     * the node order in the hierarchy, so we skip the indexing when a limit
     * is given. However, if an order is given together with the limit, then
     * we can use indexing because the final result is sorted and limited after
     * the node filtering.
     */
    if (nr_index_hints > 0 && limit != -1 && order == HIERARCHY_RESULT_ORDER_NONE) {
        nr_index_hints = 0;
    }

    /*
     * Run for each NODE_ID.
     */
    ssize_t nr_nodes = 0;
    size_t merge_nr_fields = 0;
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
        struct SelvaSet *ind_out[max(nr_index_hints, 1)];
        int ind_select = -1; /* Selected index. The smallest of all found. */

        memset(ind_icb, 0, nr_index_hints * sizeof(struct SelvaFindIndexControlBlock *));
        memset(ind_out, 0, nr_index_hints * sizeof(struct SelvaSet *));

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
            for (int j = 0; j < nr_index_hints; j++) {
                struct SelvaFindIndexControlBlock *icb = NULL;
                struct SelvaSet *set = NULL;
                int ind_err;

                ind_err = SelvaFind_AutoIndex(ctx, hierarchy, dir, dir_expr, nodeId, index_hints[j], &icb, &set);
                ind_icb[j] = icb;
                if (!ind_err) {
                    ind_out[j] = set;

                    if (ind_select < 0 || SelvaSet_Size(set) < SelvaSet_Size(ind_out[ind_select])) {
                        ind_select = j; /* Select the smallest index res set for fastest lookup. */
                    }
                } else if (ind_err != SELVA_ENOENT) {
                    fprintf(stderr, "%s:%d: AutoIndex returned an error: %s\n",
                            __FILE__, __LINE__,
                            getSelvaErrorStr(ind_err));
                }
            }
        }

        /*
         * Run BFS/DFS.
         */
        ssize_t tmp_limit = -1;
        const size_t skip = ind_select >= 0 ? 0 : SelvaTraversal_GetSkip(dir); /* Skip n nodes from the results. */
        struct FindCommand_Args args = {
            .ctx = ctx,
            .lang = lang,
            .hierarchy = hierarchy,
            .nr_nodes = &nr_nodes,
            .offset = (order == HIERARCHY_RESULT_ORDER_NONE) ? offset + skip : skip,
            .limit = (order == HIERARCHY_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
            .merge_strategy = merge_strategy,
            .merge_path = merge_path,
            .merge_nr_fields = &merge_nr_fields,
            .fields = fields,
            .fields_rpn_ctx = fields_rpn_ctx,
            .fields_expression = fields_expression,
            .excluded_fields = excluded_fields,
            .order_field = order_by_field,
            .order_result = &order_result,
            .acc_tot = 0,
            .acc_take = 0,
        };
        const struct SelvaHierarchyCallback cb = {
            .node_cb = FindCommand_NodeCb,
            .node_arg = &args,
        };

        if (limit == 0) {
            break;
        }

        if (ind_select >= 0) {
            struct SelvaSetElement *el;

            /*
             * There is no need to run the filter again if the indexing was
             * executing the same filter already.
             */
            if (argv_filter_expr && !RedisModule_StringCompare(argv_filter_expr, index_hints[ind_select])) {
                args.rpn_ctx = NULL;
                args.filter = NULL;
            }

            SELVA_TRACE_BEGIN(cmd_find_index);
            SELVA_SET_NODEID_FOREACH(el, ind_out[ind_select]) {
                struct SelvaHierarchyNode *node;

                node = SelvaHierarchy_FindNode(hierarchy, el->value_nodeId);
                if (node) {
                    /*
                     * Note that we don't break here on limit because limit and
                     * indexing aren't compatible, unless limit is used together
                     * with order.
                     */
                    (void)FindCommand_NodeCb(node, &args);
                }
            }
            SELVA_TRACE_END(cmd_find_index);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_ARRAY && ref_field) {
            const struct SelvaModify_ArrayObjectCallback ary_cb = {
                .node_cb = FindCommand_ArrayNodeCb,
                .node_arg = &args,
            };
            TO_STR(ref_field);

            SELVA_TRACE_BEGIN(cmd_find_array);
            err = SelvaModify_TraverseArray(hierarchy, nodeId, ref_field_str, ref_field_len, &ary_cb);
            SELVA_TRACE_END(cmd_find_array);
        } else if ((dir & (SELVA_HIERARCHY_TRAVERSAL_REF |
                    SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                    SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD))
                   && ref_field) {
            TO_STR(ref_field);

            SELVA_TRACE_BEGIN(cmd_find_refs);
            err = SelvaHierarchy_TraverseField(hierarchy, nodeId, dir, ref_field_str, ref_field_len, &cb);
            SELVA_TRACE_END(cmd_find_refs);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
            SELVA_TRACE_BEGIN(cmd_find_bfs_expression);
            err = SelvaHierarchy_TraverseExpressionBfs(ctx, hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, &cb);
            SELVA_TRACE_END(cmd_find_bfs_expression);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_EXPRESSION) {
            SELVA_TRACE_BEGIN(cmd_find_traversal_expression);
            err = SelvaHierarchy_TraverseExpression(ctx, hierarchy, nodeId, traversal_rpn_ctx, traversal_expression, &cb);
            SELVA_TRACE_END(cmd_find_traversal_expression);
        } else {
            SELVA_TRACE_BEGIN(cmd_find_rest);
            err = SelvaHierarchy_Traverse(hierarchy, nodeId, dir, &cb);
            SELVA_TRACE_END(cmd_find_rest);
        }
        for (int j = 0; j < nr_index_hints; j++) {
            struct SelvaFindIndexControlBlock *icb = ind_icb[j];

            if (!icb) {
                continue;
            }

            if (j == ind_select) {
                SelvaFind_Acc(icb, args.acc_take, args.acc_tot);
            } else if (ind_select == -1) {
                /* No index was selected so all will get the same take. */
                SelvaFind_Acc(icb, args.acc_take, args.acc_tot);
            } else {
                /* Nothing taken from this index. */
                SelvaFind_Acc(icb, 0, args.acc_tot);
            }
        }
        if (err != 0) {
            /*
             * We can't send an error to the client at this point so we'll just log
             * it and ignore the error.
             */
            fprintf(stderr, "%s:%d: Find failed. err: %s dir: %s node_id: \"%.*s\"\n",
                    __FILE__, __LINE__,
                    getSelvaErrorStr(err),
                    SelvaTraversal_Dir2str(dir),
                    (int)SELVA_NODE_ID_SIZE, nodeId);
        }
    }

    /*
     * If an ordered request was requested then nothing was send to the client yet
     * and we need to do it now.
     */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        SELVA_TRACE_BEGIN(cmd_find_sort_result);
        nr_nodes = (dir == SELVA_HIERARCHY_TRAVERSAL_ARRAY)
            ? FindCommand_PrintOrderedArrayResult(ctx, lang, offset, limit, fields, &order_result)
            : FindCommand_PrintOrderedResult(ctx, lang, hierarchy, offset, limit, merge_strategy, merge_path,
                                             fields, excluded_fields, &order_result, &merge_nr_fields);
        SELVA_TRACE_END(cmd_find_sort_result);
    }

    /* nr_nodes is never negative at this point so we can safely cast it. */
    RedisModule_ReplySetArrayLength(ctx, (merge_strategy == MERGE_STRATEGY_NONE) ? (size_t)nr_nodes : merge_nr_fields);

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

/**
 * Find node in set.
 * SELVA.HIERARCHY.findIn REDIS_KEY [order field asc|desc] [offset 1234] [limit 1234] [fields field1\nfield2] NODE_IDS [expression] [args...]
 */
int SelvaHierarchy_FindInCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_LANG      = 1;
    const int ARGV_REDIS_KEY = 2;
    const int ARGV_ORDER_TXT = 3;
    const int ARGV_ORDER_FLD = 4;
    const int ARGV_ORDER_ORD = 5;
    int ARGV_OFFSET_TXT      = 3;
    int ARGV_OFFSET_NUM      = 4;
    int ARGV_LIMIT_TXT       = 3;
    int ARGV_LIMIT_NUM       = 4;
    int ARGV_FIELDS_TXT      = 3;
    int ARGV_FIELDS_VAL      = 4;
    int ARGV_NODE_IDS        = 3;
    int ARGV_FILTER_EXPR     = 4;
    int ARGV_FILTER_ARGS     = 5;
#define SHIFT_ARGS(i) \
    ARGV_OFFSET_TXT += i; \
    ARGV_OFFSET_NUM += i; \
    ARGV_LIMIT_TXT += i; \
    ARGV_LIMIT_NUM += i; \
    ARGV_FIELDS_TXT += i; \
    ARGV_FIELDS_VAL += i; \
    ARGV_NODE_IDS += i; \
    ARGV_FILTER_EXPR += i; \
    ARGV_FILTER_ARGS += i

    if (argc < 5) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *lang = argv[ARGV_LANG];

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Parse the order arg.
     */
    enum SelvaResultOrder order = HIERARCHY_RESULT_ORDER_NONE;
    const RedisModuleString *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = SelvaTraversal_ParseOrder(&order_by_field, &order,
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
     * Parse fields.
     */
    selvaobject_autofree struct SelvaObject *fields = NULL;
    RedisModuleString *excluded_fields = NULL;
    if (argc > ARGV_FIELDS_VAL) {
        err = SelvaArgsParser_StringSetList(ctx, &fields, &excluded_fields, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
        if (err == 0) {
            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "fields");
        }
    }

    size_t nr_reg = argc - ARGV_FILTER_ARGS + 1;
    struct rpn_ctx *rpn_ctx = rpn_init(nr_reg);
    if (!rpn_ctx) {
        return replyWithSelvaError(ctx, SELVA_ENOMEM);
    }

    const RedisModuleString *ids = argv[ARGV_NODE_IDS];
    const RedisModuleString *filter = argv[ARGV_FILTER_EXPR];
    TO_STR(ids, filter);

    /*
     * Compile the filter expression.
     */
    struct rpn_expression *filter_expression = rpn_compile(filter_str);
    if (!filter_expression) {
        rpn_destroy(rpn_ctx);
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

    SVECTOR_AUTOFREE(order_result); /*!< for ordered result. */
    if (order != HIERARCHY_RESULT_ORDER_NONE &&
        !SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, SelvaTraversal_GetOrderFunc(order))) {
        replyWithSelvaError(ctx, SELVA_ENOMEM);
        goto out;
    }

    ssize_t array_len = 0;
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run the filter for each node.
     */
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        struct SelvaHierarchyNode *node;
        ssize_t tmp_limit = -1;
        struct FindCommand_Args args = {
            .ctx = ctx,
            .lang = lang,
            .hierarchy = hierarchy,
            .nr_nodes = &array_len,
            .offset = (order == HIERARCHY_RESULT_ORDER_NONE) ? offset : 0,
            .limit = (order == HIERARCHY_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
            .rpn_ctx = rpn_ctx,
            .filter = filter_expression,
            .merge_strategy = MERGE_STRATEGY_NONE,
            .merge_path = NULL,
            .merge_nr_fields = 0,
            .fields = fields,
            .excluded_fields = excluded_fields,
            .order_field = order_by_field,
            .order_result = &order_result,
            .acc_take = 0,
            .acc_tot = 0,
        };

        node = SelvaHierarchy_FindNode(hierarchy, ids_str + i);
        if (node) {
            (void)FindCommand_NodeCb(node, &args);
        }
    }

    /*
     * If an ordered request was requested then nothing was sent to the client yet
     * and we need to do it now.
     */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        array_len = FindCommand_PrintOrderedResult(ctx, lang, hierarchy, offset, limit, MERGE_STRATEGY_NONE, NULL,
                                                   fields, excluded_fields, &order_result, NULL);
    }

    RedisModule_ReplySetArrayLength(ctx, array_len);

out:
    rpn_destroy(rpn_ctx);
#if MEM_DEBUG
    memset(filter_expression, 0, sizeof(*filter_expression));
#endif
    rpn_destroy_expression(filter_expression);

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

static int Find_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.find", SelvaHierarchy_FindCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.findIn", SelvaHierarchy_FindInCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Find_OnLoad);
