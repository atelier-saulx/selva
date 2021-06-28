#pragma once
#ifndef SELVA_TRAVERSAL
#define SELVA_TRAVERSAL

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "redismodule.h"
#include "errors.h"
#include "selva.h"
#include "arg_parser.h"
#include "hierarchy.h"
#include "selva_object.h"
#include "cdefs.h"
#include "selva_lang.h"

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

enum FindCommand_OrderedItemType {
    ORDERED_ITEM_TYPE_EMPTY,
    ORDERED_ITEM_TYPE_TEXT,
    ORDERED_ITEM_TYPE_DOUBLE,
};

struct FindCommand_OrderedItem {
    Selva_NodeId id;
    enum FindCommand_OrderedItemType type;
    struct SelvaObject *data_obj;
    double d;
    size_t data_len;
    char data[];
};

enum hierarchy_result_order {
    HIERARCHY_RESULT_ORDER_NONE,
    HIERARCHY_RESULT_ORDER_ASC,
    HIERARCHY_RESULT_ORDER_DESC,
};

typedef int (*orderFunc)(const void ** restrict a_raw, const void ** restrict b_raw);

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

static const char *get_next_field_name(const char *s)
{
    while (*s != '\n' && *s != '\0') s++;
    return s;
}

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

    memset(data_lang, '\0', sizeof(data_lang));
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

static struct FindCommand_OrderedItem *createFindCommand_ObjectBasedOrderItem(RedisModuleCtx *ctx, RedisModuleString *lang, struct SelvaObject *obj, const RedisModuleString *order_field) {
    struct FindCommand_OrderedItem *item = NULL;
    double d = 0.0;
    char data_lang[8];
    const char *data = NULL;
    size_t data_len = 0;
    enum FindCommand_OrderedItemType type = ORDERED_ITEM_TYPE_EMPTY;

    memset(data_lang, '\0', sizeof(data_lang));

    RedisModuleString *value = NULL;
    int err;

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

    memcpy(item->id, EMPTY_NODE_ID, SELVA_NODE_ID_SIZE);
    item->type = type;
    if (type == ORDERED_ITEM_TYPE_TEXT && data_len > 0) {
        strxfrm_l(item->data, data, final_data_len + 1, locale);
    }
    item->data_len = data_len;
    item->d = d;
    item->data_obj = obj;

cleanup:
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


#endif /* SELVA_TRAVERSAL */
