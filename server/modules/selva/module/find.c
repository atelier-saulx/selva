#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "redismodule.h"
#include "arg_parser.h"
#include "errors.h"
#include "hierarchy.h"
#include "rpn.h"
#include "selva.h"
#include "selva_lang.h"
#include "selva_node.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "subscriptions.h"
#include "edge.h"
#include "svector.h"
#include "cstrings.h"

enum SelvaTraversalAlgo {
    HIERARCHY_BFS,
    HIERARCHY_DFS,
};

enum merge_strategy {
    MERGE_STRATEGY_NONE = 0, /* No merge. */
    MERGE_STRATEGY_ALL,
    MERGE_STRATEGY_NAMED,
    MERGE_STRATEGY_DEEP,
};

/*
 * Note that only the merge args supported by the query syntax needs to be
 * listed here. Specifically MERGE_STRATEGY_NAMED is implicit and
 * MERGE_STRATEGY_NONE is redundant.
 */
static const struct SelvaArgParser_EnumType merge_types[] = {
    {
        .name = "merge",
        .id = MERGE_STRATEGY_ALL,
    },
    {
        .name = "deepMerge",
        .id = MERGE_STRATEGY_DEEP,
    },
    {
        .name = NULL,
        .id = 0,
    }
};

struct FindCommand_Args {
    RedisModuleCtx *ctx;
    RedisModuleString *lang;
    SelvaModify_Hierarchy *hierarchy;

    ssize_t *nr_nodes; /*!< Number of nodes in the result. */
    ssize_t offset; /*!< Start from nth node. */
    ssize_t *limit; /*!< Limit the number of result. */

    struct rpn_ctx *rpn_ctx;
    const struct rpn_expression *filter;

    enum merge_strategy merge_strategy;
    RedisModuleString *merge_path;
    size_t *merge_nr_fields;

    /**
     * Field names.
     * If set the callback should return the value of these fields instead of
     * node IDs.
     *
     * fields selected in cmd args:
     * ```
     * {
     *   '0': ['field1', 'field2'],
     *   '1': ['field3', 'field4'],
     * }
     * ```
     *
     * merge && no fields selected in cmd args:
     * {
     * }
     *
     * and the final callback will use this as a scratch space to mark which
     * fields have been already sent.
     */
    struct SelvaObject *fields;

    const RedisModuleString *order_field; /*!< Order by field name; Otherwise NULL. */
    SVector *order_result; /*!< Results of the find wrapped in FindCommand_OrderedItem structs. Only used if sorting is requested. */

    struct Selva_SubscriptionMarker *marker; /*!< Used by FindInSub. */
};

enum hierarchy_result_order {
    HIERARCHY_RESULT_ORDER_NONE,
    HIERARCHY_RESULT_ORDER_ASC,
    HIERARCHY_RESULT_ORDER_DESC,
};

enum FindCommand_OrderedItemType {
    ORDERED_ITEM_TYPE_EMPTY,
    ORDERED_ITEM_TYPE_TEXT,
    ORDERED_ITEM_TYPE_DOUBLE,
};

struct FindCommand_OrderedItem {
    Selva_NodeId id;
    enum FindCommand_OrderedItemType type;
    double d;
    size_t data_len;
    char data[];
};

typedef int (*orderFunc)(const void ** restrict a_raw, const void ** restrict b_raw);

/*
 * txt = "order"
 * fld = field_name
 * ord = asc|desc
 */
static int parse_order(
        const RedisModuleString **order_by_field,
        enum hierarchy_result_order *order,
        const RedisModuleString *txt,
        const RedisModuleString *fld,
        const RedisModuleString *ord) {
    TO_STR(txt, fld, ord);
    enum hierarchy_result_order tmpOrder;

    if (strcmp("order", txt_str)) {
        return SELVA_HIERARCHY_ENOENT;
    }
    if (unlikely(ord_len < 3)) {
        goto einval;
    }

    if (ord_str[0] == 'a' && !strcmp("asc", ord_str)) {
        tmpOrder = HIERARCHY_RESULT_ORDER_ASC;
    } else if (ord_str[0] == 'd' && !strcmp("desc", ord_str)) {
        tmpOrder = HIERARCHY_RESULT_ORDER_DESC;
    } else {
einval:
        fprintf(stderr, "%s:%d: Invalid order \"%.*s\"\n",
                __FILE__, __LINE__,
                (int)ord_len, ord_str);
        return SELVA_HIERARCHY_EINVAL;
    }

    if (fld_len == 0 || fld_str[0] == '\0') {
        tmpOrder = HIERARCHY_RESULT_ORDER_NONE;
        *order_by_field = NULL;
    } else {
        *order_by_field = fld;
    }

    *order = tmpOrder;

    return 0;
}

static int parse_algo(enum SelvaTraversalAlgo *algo, const RedisModuleString *arg) {
    size_t len;
    const char *str = RedisModule_StringPtrLen(arg, &len);

    if (!strcmp("bfs", str)) {
        *algo = HIERARCHY_BFS;
    } else if (!strcmp("dfs", str)) {
        *algo = HIERARCHY_DFS;
    } else {
        return SELVA_HIERARCHY_ENOTSUP;
    }

    return 0;
}

static const char *get_next_field_name(const char *s)
{
    while (*s != '\n' && *s != '\0') s++;
    return s;
}

int parse_dir(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        enum SelvaModify_HierarchyTraversal *dir,
        RedisModuleString **field_name_out,
        Selva_NodeId nodeId,
        enum SelvaTraversalAlgo algo,
        const RedisModuleString *field_name) {
    const char *p1 = RedisModule_StringPtrLen(field_name, NULL); /* Beginning of a field_name or a list of field_names. */
    const char *p2 = get_next_field_name(p1); /* Last char of the first field_name. */

    /*
     * Open the node object.
     */
    RedisModuleString *rms_node_id;
    RedisModuleKey *node_key;
    struct SelvaObject *obj;
    int err = 0;

    /*
     * Open the node key.
     * We may not need this but we don't know it yet.
     */
    rms_node_id = RedisModule_CreateString(ctx, nodeId, Selva_NodeIdLen(nodeId));
    node_key = RedisModule_OpenKey(ctx, rms_node_id, REDISMODULE_READ | REDISMODULE_OPEN_KEY_NOTOUCH);
    err = SelvaObject_Key2Obj(node_key, &obj);
    if (err) {
        return err;
    }

    do {
        const size_t sz = (size_t)((ptrdiff_t)p2 - (ptrdiff_t)p1);

        if (sz == 4 && !strncmp("node", p1, 4)) {
            *dir = SELVA_HIERARCHY_TRAVERSAL_NODE;
            break;
        } else if (sz == 8 && !strncmp("children", p1, 8)) {
            *dir = SELVA_HIERARCHY_TRAVERSAL_CHILDREN;
            break;
        } else if (sz == 7 && !strncmp("parents", p1, 7)) {
            *dir = SELVA_HIERARCHY_TRAVERSAL_PARENTS;
            break;
        } else if (sz == 9 && !strncmp("ancestors", p1, 9)) {
            *dir = algo == HIERARCHY_BFS
                ? SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS
                : SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS;
            break;
        } else if (sz == 11 && !strncmp("descendants", p1, 11)) {
            *dir = algo == HIERARCHY_BFS
                ? SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS
                : SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS;
            break;
        } else if (sz > 0) {
            /*
             * Check if the field_name is a custom edge field name.
             * TODO This should check that the field is non-empty.
             */
            if (Edge_GetField(SelvaHierarchy_FindNode(hierarchy, nodeId), p1, sz)) {
                RedisModuleString *rms;

                if (algo != HIERARCHY_BFS) {
                    err = SELVA_HIERARCHY_EINVAL;
                    break;
                }

                rms = RedisModule_CreateString(ctx, p1, sz);
                if (!rms) {
                    err = SELVA_HIERARCHY_ENOMEM;
                    break;
                }

                err = 0;
                *field_name_out = rms;
                *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD;
                break;
            } else {
                /*
                 * Check if the field_name is a regular field containing
                 * a set of nodeId strings.
                 */
                enum SelvaObjectType type;

                /* TODO Actually verify that it contains strings */
                type = SelvaObject_GetTypeStr(obj, p1, sz);
                if (type == SELVA_OBJECT_SET) {
                    RedisModuleString *rms;

#if 0
                    fprintf(stderr, "%s:%d: Field exists. node: %.*s field: %.*s type: %d\n",
                            __FILE__, __LINE__,
                            (int)SELVA_NODE_ID_SIZE, nodeId,
                            (int)sz, p1,
                            type);
#endif

                    rms = RedisModule_CreateString(ctx, p1, sz);
                    if (!rms) {
                        err = SELVA_HIERARCHY_ENOMEM;
                        break;
                    }

                    err = 0;
                    *field_name_out = rms;
                    *dir = SELVA_HIERARCHY_TRAVERSAL_REF;
                    break;
                } else if (type == SELVA_OBJECT_ARRAY) {
                    RedisModuleString *rms;

#if 0
                    fprintf(stderr, "%s:%d: Field exists. node: %.*s field: %.*s type: %d\n",
                            __FILE__, __LINE__,
                            (int)SELVA_NODE_ID_SIZE, nodeId,
                            (int)sz, p1,
                            type);
#endif

                    rms = RedisModule_CreateString(ctx, p1, sz);
                    if (!rms) {
                        err = SELVA_HIERARCHY_ENOMEM;
                        break;
                    }

                    err = 0;
                    *field_name_out = rms;
                    *dir = SELVA_HIERARCHY_TRAVERSAL_ARRAY;
                    break;
                }
            }
        } else {
            err = SELVA_HIERARCHY_EINVAL;
            break;
        }

        if (*p2 == '\0') {
            /* If this was the last field_name, we give up. */
            err = SELVA_HIERARCHY_ENOENT;
            break;
        }

        /* Find the next field_name in the string. */
        p1 = p2 + 1;
        p2 = get_next_field_name(p1);
    } while (p1 != p2);

    RedisModule_CloseKey(node_key);
    return err;
}

static int FindCommand_compareNone(const void ** restrict a_raw __unused, const void ** restrict b_raw __unused) {
    return 0;
}

static int FindCommand_compareAsc(const void ** restrict a_raw, const void ** restrict b_raw) {
    const struct FindCommand_OrderedItem *a = *(const struct FindCommand_OrderedItem **)a_raw;
    const struct FindCommand_OrderedItem *b = *(const struct FindCommand_OrderedItem **)b_raw;
    const char *aStr = a->data;
    const char *bStr = b->data;

    if (a->type == ORDERED_ITEM_TYPE_DOUBLE &&
        b->type == ORDERED_ITEM_TYPE_DOUBLE) {
        double x = a->d;
        double y = b->d;

        if (x < y) {
            return -1;
        } else if (x > y) {
            return 1;
        }
    } else if (a->data_len && b->data_len) {
        const int res = strcmp(aStr, bStr);

        if (res != 0) {
            return res;
        }
    }

    return memcmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

static int FindCommand_compareDesc(const void ** restrict a_raw, const void ** restrict b_raw) {
    return FindCommand_compareAsc(b_raw, a_raw);
}

static orderFunc getOrderFunc(enum hierarchy_result_order order) {
    switch (order) {
    case HIERARCHY_RESULT_ORDER_ASC:
        return FindCommand_compareAsc;
    case HIERARCHY_RESULT_ORDER_DESC:
        return FindCommand_compareDesc;
    case HIERARCHY_RESULT_ORDER_NONE:
    default:
        return FindCommand_compareNone;
    }
}

static struct FindCommand_OrderedItem *createFindCommand_OrderItem(RedisModuleCtx *ctx, RedisModuleString *lang, struct SelvaModify_HierarchyNode *node, const RedisModuleString *order_field) {
    Selva_NodeId nodeId;
    RedisModuleString *id;
    RedisModuleKey *key;
    struct FindCommand_OrderedItem *item = NULL;
    double d = 0.0;
    char data_lang[8];
    const char *data = NULL;
    size_t data_len = 0;
    enum FindCommand_OrderedItemType type = ORDERED_ITEM_TYPE_EMPTY;

    SelvaModify_HierarchyGetNodeId(nodeId, node);

    id = RedisModule_CreateString(ctx, nodeId, Selva_NodeIdLen(nodeId));
    if (!id) {
        return NULL;
    }

    key = RedisModule_OpenKey(ctx, id, REDISMODULE_READ | REDISMODULE_OPEN_KEY_NOTOUCH);
    if (key) {
        struct SelvaObject *obj;
        RedisModuleString *value = NULL;
        int err;

        err = SelvaObject_Key2Obj(key, &obj);
        if (!err) {
            enum SelvaObjectType obj_type;

            obj_type = SelvaObject_GetType(obj, order_field);

            if (obj_type == SELVA_OBJECT_STRING) {
                err = SelvaObject_GetString(obj, order_field, &value);
                if (!err && value) {
                    data = RedisModule_StringPtrLen(value, &data_len);
                    type = ORDERED_ITEM_TYPE_TEXT;
                }
            } else if (obj_type == SELVA_OBJECT_OBJECT) {
                SelvaObjectMeta_t meta;
                SelvaObject_GetUserMeta(obj, order_field, &meta);

                if (meta == SELVA_OBJECT_META_SUBTYPE_TEXT) {
                    TO_STR(lang);
                    struct SelvaObject *text_obj;
                    int text_err = SelvaObject_GetObject(obj, order_field, &text_obj);
                    if (text_err) {
                        goto cleanup;
                    }

                    if (!lang_len) {
                        goto cleanup;
                    }

                    char buf[lang_len + 1];
                    memcpy(buf, lang_str, lang_len + 1);
                    const char *sep = "\n";
                    char *rest = NULL;

                    for (const char *token = strtok_r(buf, sep, &rest);
                         token != NULL;
                         token = strtok_r(NULL, sep, &rest)) {
                        const size_t slen = strlen(token);

                        RedisModuleString *raw_value = NULL;
                        text_err = SelvaObject_GetStringStr(text_obj, token, slen, &raw_value);
                        if (!text_err && raw_value) {
                            TO_STR(raw_value);

                            if (raw_value_len) {
                                value = raw_value;
                                strncpy(data_lang, token, sizeof(data_lang) - 1);
                                data_lang[sizeof(data_lang) - 1] = '\0';
                                data = raw_value_str;
                                data_len = raw_value_len;
                                type = ORDERED_ITEM_TYPE_TEXT;
                                break;
                            }
                        }
                    }
                }
            } else if (obj_type == SELVA_OBJECT_DOUBLE) {
                err = SelvaObject_GetDouble(obj, order_field, &d);
                if (!err) {
                    type = ORDERED_ITEM_TYPE_DOUBLE;
                }
            } else if (obj_type == SELVA_OBJECT_LONGLONG) {
                long long v;

                err = SelvaObject_GetLongLong(obj, order_field, &v);
                if (!err) {
                    d = (double)v;
                    type = ORDERED_ITEM_TYPE_DOUBLE;
                }
            }
        }
    }

    size_t final_data_len = data_len;
    locale_t locale = 0;

    if (type == ORDERED_ITEM_TYPE_TEXT && data_len > 0) {
        locale = SelvaLang_GetLocale(data_lang, strlen(data_lang));
        final_data_len = strxfrm_l(NULL, data, 0, locale);
    }

    item = RedisModule_PoolAlloc(ctx, sizeof(struct FindCommand_OrderedItem) + final_data_len + 1);
    if (!item) {
        /*
         * Returning NULL in case of ENOMEM here should be fairly ok as we can
         * assume that Redis will free everything we allocated and opened before
         * this point. Although it's possible that there is not enough memory to
         * do the cleanup but there is nothing we could do better neither here.
         */
        return NULL;
    }

    memcpy(item->id, nodeId, SELVA_NODE_ID_SIZE);
    item->type = type;
    if (type == ORDERED_ITEM_TYPE_TEXT && data_len > 0) {
        strxfrm_l(item->data, data, final_data_len + 1, locale);
    }
    item->data_len = data_len;
    item->d = d;

cleanup:
    RedisModule_CloseKey(key);
    return item;
}

static int fields_contains(struct SelvaObject *fields, const char *field_name_str, size_t field_name_len) {
    void *iterator;
    const SVector *vec;

    iterator = SelvaObject_ForeachBegin(fields);
    while ((vec = (SVector *)SelvaObject_ForeachValue(fields, &iterator, NULL, SELVA_OBJECT_ARRAY))) {
        struct SVectorIterator it;
        const RedisModuleString *s;

        SVector_ForeachBegin(&it, vec);
        while ((s = SVector_Foreach(&it))) {
            TO_STR(s);

            if (s_len == field_name_len && !strcmp(s_str, field_name_str)) {
                return 1;
            }
        }
    }

    return 0;
}

/* TODO MOVE this */
static int send_node_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaModify_Hierarchy *hierarchy,
        struct SelvaModify_HierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *field);

static int send_edge_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaModify_Hierarchy *hierarchy,
        struct SelvaModify_HierarchyNode *node,
        struct SelvaObject *edges,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *field) {
    struct EdgeField *edge_field;
    TO_STR(field);

    int off = SelvaObject_GetPointerPartialMatchStr(edges, field_str, field_len, &edge_field);
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

    const struct EdgeFieldConstraint* constraint = Edge_GetFieldConstraint(edge_field);
    if (!constraint->flags.single_ref) {
        return SELVA_EINTYPE;
    }

    struct SelvaModify_HierarchyNode *dst_node;
    dst_node = SVector_GetIndex(&edge_field->arcs, 0);
    if (!dst_node) {
        Selva_NodeId node_id;

        SelvaModify_HierarchyGetNodeId(node_id, node);
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

    Selva_NodeId dst_id;
    SelvaModify_HierarchyGetNodeId(dst_id, dst_node);

    RedisModuleString *dst_id_rms;
    dst_id_rms = RedisModule_CreateString(ctx, dst_id, Selva_NodeIdLen(dst_id));
    if (!dst_id_rms) {
        return SELVA_ENOMEM;
    }

    RedisModuleKey *dst_key;
    dst_key = RedisModule_OpenKey(ctx, dst_id_rms, REDISMODULE_READ | REDISMODULE_OPEN_KEY_NOTOUCH);
    if (!dst_key) {
        return SELVA_ENOENT;
    }

    struct SelvaObject *dst_obj;
    int err;
    err = SelvaObject_Key2Obj(dst_key, &dst_obj);
    if (err) {
        return err;
    }

    const char *next_field_str = field_str + off;
    size_t next_field_len = field_len - off;
    RedisModuleString *next_field = RedisModule_CreateString(ctx, next_field_str, next_field_len);
    if (!next_field) {
        return SELVA_ENOMEM;
    }
    RedisModuleString *next_prefix;

    const char *next_prefix_str;
    size_t next_prefix_len;
    if (field_prefix_str) {
        const char *s = strnstr(field_str, ".", field_len);
        const int n = s ? (int)(s - field_str) + 1 : (int)field_len;

        next_prefix = RedisModule_CreateStringPrintf(ctx, "%.*s%.*s", (int)field_prefix_len, field_prefix_str, n, field_str);
        next_prefix_str = RedisModule_StringPtrLen(next_prefix, &next_prefix_len);
    } else {
        next_prefix_str = field_str;
        next_prefix_len = off;
    }

    /* TODO could close the key */
    return (send_node_field(ctx, lang, hierarchy, dst_node, dst_obj, next_prefix_str, next_prefix_len, next_field) == 0) ? SELVA_ENOENT : 0;
}

static int send_node_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaModify_Hierarchy *hierarchy,
        struct SelvaModify_HierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_prefix_str,
        size_t field_prefix_len,
        RedisModuleString *field) {
    TO_STR(field);
    Selva_NodeId nodeId;
    int err;

    SelvaModify_HierarchyGetNodeId(nodeId, node);

    RedisModuleString *full_field_name;
    if (field_prefix_str) {
        full_field_name = RedisModule_CreateStringPrintf(ctx, "%.*s%s", (int)field_prefix_len, field_prefix_str, field_str);
        if (!full_field_name) {
            return SELVA_ENOMEM;
        }
    } else {
        full_field_name = field;
    }
#define SEND_FIELD_NAME() RedisModule_ReplyWithString(ctx, full_field_name)

    /*
     * Check if the field name is a hierarchy field name.
     */
    if (!strcmp(field_str, "ancestors")) {
        RedisModule_ReplyWithString(ctx, field);
        SEND_FIELD_NAME();
        err = HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS);
        if (err) {
            fprintf(stderr, "%s:%d: Sending the ancestors field of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    getSelvaErrorStr(err));
        }
        return 1;
    } else if (!strcmp(field_str, "children")) {
        SEND_FIELD_NAME();
        err = HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_CHILDREN);
        if (err) {
            fprintf(stderr, "%s:%d: Sending the children field of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    getSelvaErrorStr(err));
        }
        return 1;
    } else if (!strcmp(field_str, "descendants")) {
        SEND_FIELD_NAME();
        err = HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS);
        if (err) {
            fprintf(stderr, "%s:%d: Sending the descendants field of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    getSelvaErrorStr(err));
        }
        return 1;
    } else if (!strcmp(field_str, "parents")) {
        SEND_FIELD_NAME();
        err = HierarchyReply_WithTraversal(ctx, hierarchy, nodeId, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_PARENTS);
        if (err) {
            fprintf(stderr, "%s:%d: Sending the parents field of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    getSelvaErrorStr(err));
        }
        return 1;
    } else {
        /*
         * Check if the field name is an edge field.
         */
        struct SelvaModify_HierarchyMetadata *metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(node);
        struct SelvaObject *edges = metadata->edge_fields.edges;

        if (edges) {
            err = send_edge_field(ctx, lang, hierarchy, node, edges, field_prefix_str, field_prefix_len, field);
            if (!err) {
                return 1;
            }
        }
    }

    /*
     * Check if we have a wildcard in the middle of the field name
     * and process it.
     */
    if (strstr(field_str, ".*.")) {
        long resp_count = 0;
        err = SelvaObject_GetWithWildcardStr(ctx, lang, obj, field_str, field_len, &resp_count, -1, 0);
        if (err && err != SELVA_ENOENT) {
            fprintf(stderr, "%s:%d: Sending wildcard field %.*s of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)field_len, field_str,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    getSelvaErrorStr(err));
        }

        return resp_count / 2;
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
    SEND_FIELD_NAME();
    err = SelvaObject_ReplyWithObject(ctx, lang, obj, field);
    if (err) {
        fprintf(stderr, "%s:%d: Failed to send the field (%s) for node_id: \"%.*s\" err: \"%s\"\n",
                __FILE__, __LINE__,
                field_str,
                (int)SELVA_NODE_ID_SIZE, nodeId,
                getSelvaErrorStr(err));
        RedisModule_ReplyWithNull(ctx);
    }

    return 1;
}

static int send_node_fields(RedisModuleCtx *ctx, RedisModuleString *lang, SelvaModify_Hierarchy *hierarchy, struct SelvaModify_HierarchyNode *node, struct SelvaObject *fields) {
    Selva_NodeId nodeId;
    RedisModuleString *id;
    int err;

    SelvaModify_HierarchyGetNodeId(nodeId, node);

    id = RedisModule_CreateString(ctx, nodeId, Selva_NodeIdLen(nodeId));
    if (!id) {
        return SELVA_ENOMEM;
    }

    RedisModuleKey *key;
    key = RedisModule_OpenKey(ctx, id, REDISMODULE_READ | REDISMODULE_OPEN_KEY_NOTOUCH);
    if (!key) {
        return SELVA_ENOENT;
    }

    struct SelvaObject *obj;
    err = SelvaObject_Key2Obj(key, &obj);
    if (err) {
        return err;
    }

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
    RedisModule_ReplyWithString(ctx, id);

    const ssize_t fields_len = SelvaObject_Len(fields, NULL);
    if (fields_len < 0) {
        RedisModule_CloseKey(key);

        return fields_len;
    } else if (fields_len == 1 && fields_contains(fields, "*", 1)) { /* '*' is a wildcard */
        err = SelvaObject_ReplyWithObject(ctx, lang, obj, NULL);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to send all fields for node_id: \"%.*s\"\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId);
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

                res = send_node_field(ctx, lang, hierarchy, node, obj, NULL, 0, field);
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

    RedisModule_CloseKey(key);
    return 0;
}

static int send_array_object_field(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        SelvaModify_Hierarchy *hierarchy,
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
#define SEND_FIELD_NAME() RedisModule_ReplyWithString(ctx, full_field_name)

    /*
     * Check if the field name is a hierarchy field name.
     */
    if (!strcmp(field_str, "ancestors")) {
        RedisModule_ReplyWithString(ctx, field);
        SEND_FIELD_NAME();
        err = HierarchyReply_WithTraversal(ctx, hierarchy, EMPTY_NODE_ID, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS);
        if (err) {
            fprintf(stderr, "%s:%d: Sending the ancestors field of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, EMPTY_NODE_ID,
                    getSelvaErrorStr(err));
        }
        return 1;
    } else if (!strcmp(field_str, "children")) {
        SEND_FIELD_NAME();
        err = HierarchyReply_WithTraversal(ctx, hierarchy, EMPTY_NODE_ID, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_CHILDREN);
        if (err) {
            fprintf(stderr, "%s:%d: Sending the children field of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, EMPTY_NODE_ID,
                    getSelvaErrorStr(err));
        }
        return 1;
    } else if (!strcmp(field_str, "descendants")) {
        SEND_FIELD_NAME();
        err = HierarchyReply_WithTraversal(ctx, hierarchy, EMPTY_NODE_ID, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS);
        if (err) {
            fprintf(stderr, "%s:%d: Sending the descendants field of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, EMPTY_NODE_ID,
                    getSelvaErrorStr(err));
        }
        return 1;
    } else if (!strcmp(field_str, "parents")) {
        SEND_FIELD_NAME();
        err = HierarchyReply_WithTraversal(ctx, hierarchy, EMPTY_NODE_ID, 0, NULL, SELVA_HIERARCHY_TRAVERSAL_PARENTS);
        if (err) {
            fprintf(stderr, "%s:%d: Sending the parents field of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, EMPTY_NODE_ID,
                    getSelvaErrorStr(err));
        }
        return 1;
    } else {
        // TODO: should this be possible? I don't see how
        /*
         * Check if the field name is an edge field.
         */
        // struct SelvaModify_HierarchyMetadata *metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(node);
        // struct SelvaObject *edges = metadata->edge_fields.edges;

        // if (edges) {
        //     err = send_edge_field(ctx, lang, hierarchy, node, edges, field_prefix_str, field_prefix_len, field);
        //     if (!err) {
        //         return 1;
        //     }
        // }
    }

    /*
     * Check if we have a wildcard in the middle of the field name
     * and process it.
     */
    if (strstr(field_str, ".*.")) {
        long resp_count = 0;
        err = SelvaObject_GetWithWildcardStr(ctx, lang, obj, field_str, field_len, &resp_count, -1, 0);
        if (err && err != SELVA_ENOENT) {
            fprintf(stderr, "%s:%d: Sending wildcard field %.*s of %.*s failed: %s\n",
                    __FILE__, __LINE__,
                    (int)field_len, field_str,
                    (int)SELVA_NODE_ID_SIZE, EMPTY_NODE_ID,
                    getSelvaErrorStr(err));
        }

        return resp_count / 2;
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
    SEND_FIELD_NAME();
    err = SelvaObject_ReplyWithObject(ctx, lang, obj, field);
    if (err) {
        fprintf(stderr, "%s:%d: Failed to send the field (%s) for node_id: \"%.*s\" err: \"%s\"\n",
                __FILE__, __LINE__,
                field_str,
                (int)SELVA_NODE_ID_SIZE, EMPTY_NODE_ID,
                getSelvaErrorStr(err));
        RedisModule_ReplyWithNull(ctx);
    }

    return 1;
}


static int send_array_object_fields(RedisModuleCtx *ctx, RedisModuleString *lang, SelvaModify_Hierarchy *hierarchy, struct SelvaObject *obj, struct SelvaObject *fields) {
    RedisModuleString *id;
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
    RedisModule_ReplyWithString(ctx, id);

    const ssize_t fields_len = SelvaObject_Len(fields, NULL);
    if (fields_len < 0) {
        return fields_len;
    } else if (fields_len == 1 && fields_contains(fields, "*", 1)) { /* '*' is a wildcard */
        err = SelvaObject_ReplyWithObject(ctx, lang, obj, NULL);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to send all fields for selva object in array\n",
                    __FILE__, __LINE__);
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

                res = send_array_object_field(ctx, lang, hierarchy, obj, NULL, 0, field);
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

        RedisModuleString *key_name;
        key_name = RedisModule_CreateString(ctx, key_name_str, key_name_len);
        if (!key_name) {
            return SELVA_ENOMEM;
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
        err = SelvaObject_ReplyWithObject(ctx, lang, obj, key_name);
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
        (void)SelvaObject_SetLongLong(fields, key_name, 1);
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
    SVector *vec;
    const char *field_index;
    TO_STR(obj_path);

    iterator = SelvaObject_ForeachBegin(fields);
    while ((vec = (SVector *)SelvaObject_ForeachValue(fields, &iterator, &field_index, SELVA_OBJECT_ARRAY))) {
        struct SVectorIterator it;
        RedisModuleString *field;

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
            err = SelvaObject_ReplyWithObject(ctx, lang, obj, field);
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
        RedisModuleString *key_name;
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

            key_name = RedisModule_CreateString(ctx, key_name_str, key_name_len);
            if (!key_name) {
                return SELVA_ENOMEM;
            }

            ++*nr_fields_out;

            /*
             * Start a new array reply:
             * [node_id, field_name, field_value]
             */
            RedisModule_ReplyWithArray(ctx, 3);

            RedisModule_ReplyWithStringBuffer(ctx, nodeId, Selva_NodeIdLen(nodeId));
            RedisModule_ReplyWithString(ctx, next_path);
            err = SelvaObject_ReplyWithObject(ctx, lang, obj, key_name);
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
        Selva_NodeId nodeId,
        enum merge_strategy merge_strategy,
        RedisModuleString *obj_path,
        struct SelvaObject *fields,
        size_t *nr_fields_out) {
    RedisModuleString *id;
    TO_STR(obj_path);
    int err;

    id = RedisModule_CreateString(ctx, nodeId, Selva_NodeIdLen(nodeId));
    if (!id) {
        return SELVA_ENOMEM;
    }

    RedisModuleKey *key;
    key = RedisModule_OpenKey(ctx, id, REDISMODULE_READ | REDISMODULE_OPEN_KEY_NOTOUCH);
    if (!key) {
        return SELVA_ENOENT;
    }

    struct SelvaObject *node_obj;
    err = SelvaObject_Key2Obj(key, &node_obj);
    if (err) {
        return err;
    }

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
            err = SelvaObject_ReplyWithObject(ctx, lang, obj, NULL);
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

    RedisModule_CloseKey(key);
    return res;
}

static int FindCommand_NodeCb(struct SelvaModify_HierarchyNode *node, void *arg) {
    Selva_NodeId nodeId;
    struct FindCommand_Args *args = (struct FindCommand_Args *)arg;
    struct rpn_ctx *rpn_ctx = args->rpn_ctx;
    int take = (args->offset > 0) ? !args->offset-- : 1;

    SelvaModify_HierarchyGetNodeId(nodeId, node);

    if (take && rpn_ctx) {
        int err;

        /* Set node_id to the register */
        rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, 0);

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

        if (!sort) {
            ssize_t *nr_nodes = args->nr_nodes;
            ssize_t * restrict limit = args->limit;
            int err;

            if (args->merge_strategy != MERGE_STRATEGY_NONE) {
                err = send_node_object_merge(args->ctx, args->lang, nodeId, args->merge_strategy, args->merge_path, args->fields, args->merge_nr_fields);
            } else if (args->fields) {
                err = send_node_fields(args->ctx, args->lang, args->hierarchy, node, args->fields);
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
            struct FindCommand_OrderedItem *item;

            item = createFindCommand_OrderItem(args->ctx, args->lang, node, args->order_field);
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

        /* Set node_id to the register */
        rpn_set_reg_slvobj(rpn_ctx, 0, obj, 0);

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
                err = send_array_object_fields(args->ctx, args->lang, args->hierarchy, obj, args->fields);
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
            struct FindCommand_OrderedItem *item;

            // item = createFindCommand_OrderItem(args->ctx, args->lang, node, args->order_field);
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

static int FindInSubCommand_NodeCb(struct SelvaModify_HierarchyNode *node, void *arg) {
    Selva_NodeId nodeId;
    struct SelvaModify_HierarchyMetadata *metadata;
    struct FindCommand_Args *args = (struct FindCommand_Args *)arg;
    struct Selva_SubscriptionMarker *marker = args->marker;
    struct rpn_ctx *rpn_ctx = args->rpn_ctx;
    int take = (args->offset > 0) ? !args->offset-- : 1;

    SelvaModify_HierarchyGetNodeId(nodeId, node);
    metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(node);
    Selva_Subscriptions_SetMarker(nodeId, metadata, marker);

    if (take && rpn_ctx) {
        int err;

        /* Set node_id to the register */
        rpn_set_reg(rpn_ctx, 0, nodeId, SELVA_NODE_ID_SIZE, 0);

        /*
         * Resolve the expression and get the result.
         */
        err = rpn_bool(args->ctx, rpn_ctx, args->filter, &take);
        if (err) {
            fprintf(stderr, "%s:%d: Expression failed (node: \"%.*s\"): \"%s\"\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId,
                    rpn_str_error[err]);
            take = 0;
        }
    }

    if (take) {
        const int sort = !!args->order_field;

        if (!sort) {
            ssize_t * restrict limit = args->limit;

            if (*limit != 0) {
                ssize_t *nr_nodes = args->nr_nodes;

                RedisModule_ReplyWithStringBuffer(args->ctx, nodeId, Selva_NodeIdLen(nodeId));
                *nr_nodes = *nr_nodes + 1;
                *limit = *limit - 1;
            }
        } else {
            struct FindCommand_OrderedItem *item;

            item = createFindCommand_OrderItem(args->ctx, args->lang, node, args->order_field);
            if (item) {
                SVector_InsertFast(args->order_result, item);
            } else {
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
        SelvaModify_Hierarchy *hierarchy,
        ssize_t offset,
        ssize_t limit,
        enum merge_strategy merge_strategy,
        RedisModuleString *merge_path,
        struct SelvaObject *fields,
        SVector *order_result,
        size_t *nr_fields_out) {
    struct FindCommand_OrderedItem *item;
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
            err = send_node_object_merge(ctx, lang, item->id, merge_strategy, merge_path, fields, nr_fields_out);
        } else if (fields) {
            struct SelvaModify_HierarchyNode *node;

            /* TODO Consider if having hierarchy node pointers here would be better. */
            node = SelvaHierarchy_FindNode(hierarchy, item->id);
            if (node) {
                err = send_node_fields(ctx, lang, hierarchy, node, fields);
            } else {
                err = SELVA_HIERARCHY_ENOENT;
            }
        } else {
            RedisModule_ReplyWithStringBuffer(ctx, item->id, Selva_NodeIdLen(item->id));
            err = 0;
        }
        if (err) {
            RedisModule_ReplyWithNull(ctx);
            fprintf(stderr, "%s:%d: Failed to handle field(s) of the node: \"%.*s\" err: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, item->id,
                    getSelvaErrorStr(err));
        }

        len++;
    }

    return len;
}

static int get_skip(enum SelvaModify_HierarchyTraversal dir) {
    switch (dir) {
     /*
      * Find needs to skip the head node of the traverse for some types as we
      * are not interested in the node we already know.
      */
    case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
    case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
    case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
    case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
    case SELVA_HIERARCHY_TRAVERSAL_DFS_FULL:
        return 1;
    default:
        return 0;
    }
}

/**
 * Find node(s) matching the query.
 * SELVA.HIERARCHY.find lang REDIS_KEY dfs|bfs field_name [order field asc|desc] [offset 1234] [limit 1234] [merge path] [fields field_names] NODE_IDS [expression] [args...]
 *                                     |       |          |                      |             |            |            |                    |        |            |
 * Traversal method/algo -------------/        |          |                      |             |            |            |                    |        |            |
 * Traversed field ---------------------------/           |                      |             |            |            |                    |        |            |
 * Sort order of the results ----------------------------/                       |             |            |            |                    |        |            |
 * Skip the first 1234 - 1 results ---------------------------------------------/              |            |            |                    |        |            |
 * Limit the number of results (Optional) ----------------------------------------------------/             |            |                    |        |            |
 * Merge fields. fields option must be set. ---------------------------------------------------------------/             |                    |        |            |
 * Return field values instead of node names ---------------------------------------------------------------------------/                     |        |            |
 * One or more node IDs concatenated (10 chars per ID) --------------------------------------------------------------------------------------/         |            |
 * RPN filter expression -----------------------------------------------------------------------------------------------------------------------------/             |
 * Register arguments for the RPN filter --------------------------------------------------------------------------------------------------------------------------/
 *
 * The traversed field is typically either ancestors or descendants but it can
 * be any hierarchy or edge field.
 */
static int SelvaHierarchy_Find(RedisModuleCtx *ctx, int recursive, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_LANG      = 1;
    const int ARGV_REDIS_KEY = 2;
    const int ARGV_ALGO      = 3;
    const int ARGV_DIRECTION = 4;
    const int ARGV_ORDER_TXT = 5;
    const int ARGV_ORDER_FLD = 6;
    const int ARGV_ORDER_ORD = 7;
    int ARGV_OFFSET_TXT      = 5;
    int ARGV_OFFSET_NUM      = 6;
    int ARGV_LIMIT_TXT       = 5;
    int ARGV_LIMIT_NUM       = 6;
    int ARGV_MERGE_TXT       = 5;
    int ARGV_MERGE_VAL       = 6;
    int ARGV_FIELDS_TXT      = 5;
    int ARGV_FIELDS_VAL      = 6;
    int ARGV_NODE_IDS        = 5;
    int ARGV_FILTER_EXPR     = 6;
    int ARGV_FILTER_ARGS     = 7;
#define SHIFT_ARGS(i) \
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

    if (argc < 6) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *lang = argv[ARGV_LANG];

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Select traversal method.
     */
    enum SelvaTraversalAlgo algo;
    err = parse_algo(&algo, argv[ARGV_ALGO]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "traversal method");
    }

    /*
     * Parse the order arg.
     */
    enum hierarchy_result_order order = HIERARCHY_RESULT_ORDER_NONE;
    const RedisModuleString *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = parse_order(&order_by_field, &order,
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
    enum merge_strategy merge_strategy = MERGE_STRATEGY_NONE;
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
    if (argc > ARGV_FIELDS_VAL) {
		err = SelvaArgsParser_StringSetList(ctx, &fields, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
        if (err == 0) {
            if (merge_strategy == MERGE_STRATEGY_ALL) {
                /* Having fields set turns a regular merge into a named merge. */
                merge_strategy = MERGE_STRATEGY_NAMED;
            } else if (merge_strategy != MERGE_STRATEGY_NONE) {
                return replyWithSelvaErrorf(ctx, err, "only the regular merge can be used with fields");
            }
            SHIFT_ARGS(2);
        } else if (err != SELVA_ENOENT) {
            return replyWithSelvaErrorf(ctx, err, "fields");
        }
    }
    if (merge_strategy != MERGE_STRATEGY_NONE && (!fields || fields_contains(fields, "*", 1))) {
        if (fields) {
            SelvaObject_Destroy(fields);
        }
        /* Merge needs a fields object anyway but it must be empty. */
        fields = SelvaObject_New();
    }

    struct rpn_ctx *rpn_ctx = NULL;
    struct rpn_expression *filter_expression = NULL;
    struct rpn_ctx *recursive_rpn_ctx = NULL;
    struct rpn_expression *recursive_rpn_expr = NULL;

    /*
     * Prepare the filter expression if given.
     */
    if (argc >= ARGV_FILTER_EXPR + 1) {
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
        const char *input;

        rpn_ctx = rpn_init(nr_reg);
        if (!rpn_ctx) {
            return replyWithSelvaErrorf(ctx, SELVA_ENOMEM, "filter expression");
        }

        /*
         * Compile the filter expression.
         */
        input = RedisModule_StringPtrLen(argv[ARGV_FILTER_EXPR], NULL);
        filter_expression = rpn_compile(input);
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
    }

    SVECTOR_AUTOFREE(order_result); /*!< for ordered result. */

    if (argc <= ARGV_NODE_IDS) {
        replyWithSelvaError(ctx, SELVA_HIERARCHY_EINVAL);
        goto out;
    }

    RedisModuleString *ids = argv[ARGV_NODE_IDS];
    TO_STR(ids);

    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        if (!SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, getOrderFunc(order))) {
            replyWithSelvaError(ctx, SELVA_ENOMEM);
            goto out;
        }
    }

    /*
     * In the recursive mode the direction/field_name field contains
     * an expression that should to evaluate into a set of field names.
     */
    if (recursive) {
        const char *field_selector;

        recursive_rpn_ctx = rpn_init(1);
        if (!recursive_rpn_ctx) {
            replyWithSelvaErrorf(ctx, SELVA_ENOMEM, "field selector expression");
            goto out;
        }

        field_selector = RedisModule_StringPtrLen(argv[ARGV_DIRECTION], NULL);
        recursive_rpn_expr = rpn_compile(field_selector);
        if (!recursive_rpn_expr) {
            rpn_destroy(recursive_rpn_ctx);
            replyWithSelvaErrorf(ctx, SELVA_RPN_ECOMP, "Failed to compile the field selector expression");
            goto out;
        }
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run for each NODE_ID.
     */
    ssize_t nr_nodes = 0;
    size_t merge_nr_fields = 0;
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        enum SelvaModify_HierarchyTraversal dir = SELVA_HIERARCHY_TRAVERSAL_NONE;
        Selva_NodeId nodeId;
        RedisModuleString *ref_field = NULL;

        Selva_NodeIdCpy(nodeId, ids_str + i);

        if (!recursive) {
            /*
             * Get the direction parameter.
             */
            err = parse_dir(ctx, hierarchy, &dir, &ref_field, nodeId, algo, argv[ARGV_DIRECTION]);
            if (err) {
                fprintf(stderr, "%s:%d: Error \"%s\" while selecting the field and dir for the node \"%.*s\", skipping\n",
                        __FILE__, __LINE__,
                        getSelvaErrorStr(err),
                        (int)SELVA_NODE_ID_SIZE, nodeId);
                /* Skip this node */
                continue;
            }
        } else {
            /* recursive can use this for get_skip() */
            dir = SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS;
        }

        /*
         * Run BFS/DFS.
         */
        ssize_t tmp_limit = -1;
        const size_t skip = get_skip(dir); /* Skip n nodes from the results. */
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
            .order_field = order_by_field,
            .order_result = &order_result,
        };
        const struct SelvaModify_HierarchyCallback cb = {
            .node_cb = FindCommand_NodeCb,
            .node_arg = &args,
        };

        if (limit == 0) {
            break;
        }

        if (recursive) {
            err = SelvaHierarchy_TraverseExpression(ctx, hierarchy, nodeId, recursive_rpn_ctx, recursive_rpn_expr, &cb);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_REF && ref_field) {
            TO_STR(ref_field);

            err = SelvaModify_TraverseHierarchyRef(ctx, hierarchy, nodeId, ref_field_str, &cb);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_ARRAY && ref_field) {
            // err = SelvaModify_TraverseArray(ctx, hierarchy, nodeId, ref_field_str, &cb);
        } else if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD && ref_field) {
            err = SelvaModify_TraverseHierarchyEdge(hierarchy, nodeId, ref_field, &cb);
        } else {
            err = SelvaModify_TraverseHierarchy(hierarchy, nodeId, dir, &cb);
        }
        if (err != 0) {
            /*
             * We can't send an error to the client at this point so we'll just log
             * it and ignore the error.
             */
            fprintf(stderr, "%s:%d: Find failed for node: \"%.*s\"\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, nodeId);
        }
    }

    /*
     * If an ordered request was requested then nothing was send to the client yet
     * and we need to do it now.
     */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        nr_nodes = FindCommand_PrintOrderedResult(ctx, lang, hierarchy, offset, limit, merge_strategy, merge_path, fields, &order_result, &merge_nr_fields);
    }

    /* nr_nodes is never negative at this point so we can safely cast it. */
    RedisModule_ReplySetArrayLength(ctx, (merge_strategy == MERGE_STRATEGY_NONE) ? (size_t)nr_nodes : merge_nr_fields);

out:
    if (rpn_ctx) {
        rpn_destroy(rpn_ctx);
        rpn_destroy_expression(filter_expression);
    }
    if (recursive_rpn_ctx) {
        rpn_destroy(recursive_rpn_ctx);
        rpn_destroy_expression(recursive_rpn_expr);
    }

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

int SelvaHierarchy_FindCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return SelvaHierarchy_Find(ctx, 0, argv, argc);
}

int SelvaHierarchy_FindRecursiveCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return SelvaHierarchy_Find(ctx, 1, argv, argc);
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
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Parse the order arg.
     */
    enum hierarchy_result_order order = HIERARCHY_RESULT_ORDER_NONE;
    const RedisModuleString *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = parse_order(&order_by_field, &order,
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
    if (argc > ARGV_FIELDS_VAL) {
        err = SelvaArgsParser_StringSetList(ctx, &fields, "fields", argv[ARGV_FIELDS_TXT], argv[ARGV_FIELDS_VAL]);
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
        !SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, getOrderFunc(order))) {
        replyWithSelvaError(ctx, SELVA_ENOMEM);
        goto out;
    }

    ssize_t array_len = 0;
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run the filter for each node.
     */
    for (size_t i = 0; i < ids_len; i += SELVA_NODE_ID_SIZE) {
        struct SelvaModify_HierarchyNode *node;
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
            .order_field = order_by_field,
            .order_result = &order_result,
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
        array_len = FindCommand_PrintOrderedResult(ctx, lang, hierarchy, offset, limit, MERGE_STRATEGY_NONE, NULL, fields, &order_result, NULL);
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

/**
 * Find node in set.
 * SELVA.HIERARCHY.findInSub REDIS_KEY SUB_ID MARKER_ID [order field asc|desc] [offset 1234] [limit 1234]
 */
int SelvaHierarchy_FindInSubCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const int ARGV_LANG       = 1;
    const int ARGV_REDIS_KEY  = 2;
    const int ARGV_SUB_ID     = 3;
    const int ARGV_MARKER_ID  = 4;
    const int ARGV_ORDER_TXT  = 5;
    const int ARGV_ORDER_FLD  = 6;
    const int ARGV_ORDER_ORD  = 7;
    int ARGV_OFFSET_TXT       = 5;
    int ARGV_OFFSET_NUM       = 6;
    int ARGV_LIMIT_TXT        = 5;
    int ARGV_LIMIT_NUM        = 6;
#define SHIFT_ARGS(i) \
    ARGV_OFFSET_TXT += i; \
    ARGV_OFFSET_NUM += i; \
    ARGV_LIMIT_TXT += i; \
    ARGV_LIMIT_NUM += i

    if (argc < 5) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *lang = argv[ARGV_LANG];

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Get the subscription id.
     */
    Selva_SubscriptionId sub_id;
    err = SelvaArgParser_SubscriptionId(sub_id, argv[ARGV_SUB_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "subID");
    }

    /*
     * Get the marker id.
     */
    Selva_SubscriptionMarkerId marker_id;
    err = SelvaArgParser_MarkerId(&marker_id, argv[ARGV_MARKER_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "markerID");
    }

    /*
     * Find the subscription marker.
     */
    struct Selva_SubscriptionMarker *marker;
    marker = SelvaSubscriptions_GetMarker(hierarchy, sub_id, marker_id);
    if (!marker) {
        return replyWithSelvaErrorf(ctx, SELVA_SUBSCRIPTIONS_EINVAL, "markerID");
    }

    /*
     * Parse the order arg.
     */
    enum hierarchy_result_order order = HIERARCHY_RESULT_ORDER_NONE;
    const RedisModuleString *order_by_field = NULL;
    if (argc > ARGV_ORDER_ORD) {
        err = parse_order(&order_by_field, &order,
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

    SVECTOR_AUTOFREE(order_result); /* No need to init for ORDER_NODE */
    if (order != HIERARCHY_RESULT_ORDER_NONE &&
        !SVector_Init(&order_result, (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN, getOrderFunc(order))) {
        return replyWithSelvaError(ctx, SELVA_ENOMEM);
    }

    ssize_t array_len = 0;
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    /*
     * Run the traverse function.
     */
    ssize_t tmp_limit = -1;
    size_t skip = get_skip(marker->dir); /* Skip n nodes from the results. */
    struct FindCommand_Args args = {
        .ctx = ctx,
        .lang = lang,
        .hierarchy = hierarchy,
        .nr_nodes = &array_len,
        .offset = (order == HIERARCHY_RESULT_ORDER_NONE) ? offset + skip : skip,
        .limit = (order == HIERARCHY_RESULT_ORDER_NONE) ? &limit : &tmp_limit,
        .rpn_ctx = marker->filter_ctx,
        .filter = marker->filter_expression,
        .merge_strategy = MERGE_STRATEGY_NONE,
        .merge_path = NULL,
        .merge_nr_fields = 0,
        .fields = NULL,
        .order_field = order_by_field,
        .order_result = &order_result,
        .marker = marker,
    };
    const struct SelvaModify_HierarchyCallback cb = {
        .node_cb = FindInSubCommand_NodeCb,
        .node_arg = &args,
    };

    /* RFE We expect here that all markers have a valid nodeId. */
    if (marker->dir == SELVA_HIERARCHY_TRAVERSAL_REF && marker->ref_field) {
        err = SelvaModify_TraverseHierarchyRef(ctx, hierarchy, marker->node_id, marker->ref_field, &cb);
    } else {
        /*
         * This could be implemented with a head callback but it's not
         * currently implemented for the traverse API. Therefore we do a
         * separate traverse just for the node itself in some special cases
         * where it's necessary.
         */
        if (marker->dir == SELVA_HIERARCHY_TRAVERSAL_PARENTS || marker->dir == SELVA_HIERARCHY_TRAVERSAL_CHILDREN) {
            err = SelvaModify_TraverseHierarchy(hierarchy, marker->node_id, SELVA_HIERARCHY_TRAVERSAL_NODE, &cb);
        } else {
            err = 0;
        }
        if (!err) {
            err = SelvaModify_TraverseHierarchy(hierarchy, marker->node_id, marker->dir, &cb);
        }
    }
    if (err != 0) {
        char str[SELVA_SUBSCRIPTION_ID_STR_LEN + 1];
        /*
         * We can't send an error to the client at this point so we'll just log
         * it and ignore the error.
         */
        fprintf(stderr, "%s:%d: FindInSub failed. sub_id: \"%s\" marker_id: %d err: \"%s\"\n",
                __FILE__, __LINE__,
                Selva_SubscriptionId2str(str, sub_id), (int)marker_id, selvaStrError[-err]);
    }

    /*
     * If an ordered request was requested then nothing was sent to the client yet
     * and we need to do it now.
     */
    if (order != HIERARCHY_RESULT_ORDER_NONE) {
        array_len = FindCommand_PrintOrderedResult(ctx, lang, hierarchy, offset, limit, MERGE_STRATEGY_NONE, NULL, NULL, &order_result, NULL);
    }

    RedisModule_ReplySetArrayLength(ctx, array_len);

    return REDISMODULE_OK;
#undef SHIFT_ARGS
}

static int Find_OnLoad(RedisModuleCtx *ctx) {
    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.find", SelvaHierarchy_FindCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.findRecursive", SelvaHierarchy_FindRecursiveCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.findIn", SelvaHierarchy_FindInCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.findInSub", SelvaHierarchy_FindInSubCommand, "readonly", 2, 2, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Find_OnLoad);
