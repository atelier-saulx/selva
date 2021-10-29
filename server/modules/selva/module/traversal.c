#include "redismodule.h"
#include "cdefs.h"
#include "funmap.h"
#include "selva.h"
#include "errors.h"
#include "hierarchy.h"
#include "selva_lang.h"
#include "selva_object.h"
#include "traversal.h"

/*
 * Note that only the merge args supported by the query syntax needs to be
 * listed here. Specifically MERGE_STRATEGY_NAMED is implicit and
 * MERGE_STRATEGY_NONE is redundant.
 */
const struct SelvaArgParser_EnumType merge_types[] = {
    {
        .name = "merge",
        .id = MERGE_STRATEGY_ALL,
    },
    {
        .name = "deepMerge",
        .id = MERGE_STRATEGY_DEEP,
    },
    /* Must be last. */
    {
        .name = NULL,
        .id = 0,
    }
};

int SelvaTraversal_ParseDir2(enum SelvaTraversal *dir, const RedisModuleString *arg) {
    TO_STR(arg);

    if (!strcmp("none", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_NONE;
    } else if (!strcmp("node", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_NODE;
    } else if (!strcmp("array", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_ARRAY;
    } else if (!strcmp("children", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_CHILDREN;
    } else if (!strcmp("parents", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_PARENTS;
    } else if (!strcmp("ancestors", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS;
    } else if (!strcmp("descendants", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS;
    } else if (!strcmp("ref", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_REF;
    } else if (!strcmp("edge_field", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD;
    } else if (!strcmp("bfs_edge_field", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD;
    } else if (!strcmp("bfs_expression", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION;
    } else if (!strcmp("expression", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_EXPRESSION;
    } else {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    return 0;
}

int SelvaTraversal_ParseOrder(
        const RedisModuleString **order_by_field,
        enum SelvaResultOrder *order,
        const RedisModuleString *txt,
        const RedisModuleString *fld,
        const RedisModuleString *ord) {
    TO_STR(txt, fld, ord);
    enum SelvaResultOrder tmpOrder;

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

int SelvaTraversal_CompareNone(const void ** restrict a_raw __unused, const void ** restrict b_raw __unused) {
    return 0;
}

int SelvaTraversal_CompareAsc(const void ** restrict a_raw, const void ** restrict b_raw) {
    const struct TraversalOrderedItem *a = *(const struct TraversalOrderedItem **)a_raw;
    const struct TraversalOrderedItem *b = *(const struct TraversalOrderedItem **)b_raw;
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

    return memcmp(a->node_id, b->node_id, SELVA_NODE_ID_SIZE);
}

int SelvaTraversal_CompareDesc(const void ** restrict a_raw, const void ** restrict b_raw) {
    return SelvaTraversal_CompareAsc(b_raw, a_raw);
}

static orderFunc order_functions[] = {
    [HIERARCHY_RESULT_ORDER_NONE] = SelvaTraversal_CompareNone,
    [HIERARCHY_RESULT_ORDER_ASC] = SelvaTraversal_CompareAsc,
    [HIERARCHY_RESULT_ORDER_DESC] = SelvaTraversal_CompareDesc,
};

GENERATE_FUNMAP(SelvaTraversal_GetOrderFunc, order_functions, enum SelvaResultOrder, HIERARCHY_RESULT_ORDER_NONE);

struct TraversalOrderedItem *SelvaTraversal_CreateOrderItem(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        struct SelvaModify_HierarchyNode *node,
        const RedisModuleString *order_field) {
    Selva_NodeId nodeId;
    struct TraversalOrderedItem *item = NULL;
    double d = 0.0;
    char data_lang[LANG_MAX];
    const char *data = NULL;
    size_t data_len = 0;
    enum TraversalOrderedItemType type = ORDERED_ITEM_TYPE_EMPTY;

    memset(data_lang, '\0', sizeof(data_lang));
    SelvaHierarchy_GetNodeId(nodeId, node);

    int err;
    struct SelvaObject *obj;
    obj = SelvaHierarchy_GetNodeObject(node);
    enum SelvaObjectType obj_type;

    obj_type = SelvaObject_GetType(obj, order_field);

    if (obj_type == SELVA_OBJECT_STRING) {
        RedisModuleString *value = NULL;

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
                return NULL;
            }

            if (!lang_len) {
                return NULL;
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

    item = RedisModule_PoolAlloc(ctx, sizeof(struct TraversalOrderedItem) + final_data_len + 1);
    if (!item) {
        /*
         * Returning NULL in case of ENOMEM here should be fairly ok as we can
         * assume that Redis will free everything we allocated and opened before
         * this point. Although it's possible that there is not enough memory to
         * do the cleanup but there is nothing we could do better neither here.
         */
        return NULL;
    }

    item->type = type;
    memcpy(item->node_id, nodeId, SELVA_NODE_ID_SIZE);
    item->node = node;
    if (type == ORDERED_ITEM_TYPE_TEXT && data_len > 0) {
        strxfrm_l(item->data, data, final_data_len + 1, locale);
    }
    item->data_len = final_data_len;
    item->d = d;

    return item;
}

struct TraversalOrderedItem *SelvaTraversal_CreateObjectBasedOrderItem(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        struct SelvaObject *obj,
        const RedisModuleString *order_field) {
    struct TraversalOrderedItem *item = NULL;
    double d = 0.0;
    const char *data = NULL;
    size_t data_len = 0;
    char data_lang[LANG_MAX];
    enum SelvaObjectType obj_type;
    enum TraversalOrderedItemType type = ORDERED_ITEM_TYPE_EMPTY;

    memset(data_lang, '\0', sizeof(data_lang));

    obj_type = SelvaObject_GetType(obj, order_field);
    if (obj_type == SELVA_OBJECT_STRING) {
        RedisModuleString *value = NULL;
        int err;

        err = SelvaObject_GetString(obj, order_field, &value);
        if (!err && value) {
            data = RedisModule_StringPtrLen(value, &data_len);
            type = ORDERED_ITEM_TYPE_TEXT;
        }
    } else if (obj_type == SELVA_OBJECT_OBJECT) {
        SelvaObjectMeta_t meta;
        SelvaObject_GetUserMeta(obj, order_field, &meta);

        if (meta == SELVA_OBJECT_META_SUBTYPE_TEXT) {
            struct SelvaObject *text_obj;
            TO_STR(lang);
            int err;

            if (lang_len == 0) {
                return NULL;
            }

            err = SelvaObject_GetObject(obj, order_field, &text_obj);
            if (err) {
                return NULL;
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
                err = SelvaObject_GetStringStr(text_obj, token, slen, &raw_value);
                if (!err && raw_value) {
                    TO_STR(raw_value);

                    if (raw_value_len) {
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
        int err;

        err = SelvaObject_GetDouble(obj, order_field, &d);
        if (!err) {
            type = ORDERED_ITEM_TYPE_DOUBLE;
        }
    } else if (obj_type == SELVA_OBJECT_LONGLONG) {
        long long v;
        int err;

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

    item = RedisModule_PoolAlloc(ctx, sizeof(struct TraversalOrderedItem) + final_data_len + 1);
    if (!item) {
        /*
         * Returning NULL in case of ENOMEM here should be fairly ok as we can
         * assume that Redis will free everything we allocated and opened before
         * this point. Although it's possible that there is not enough memory to
         * do the cleanup but there is nothing we could do better neither here.
         */
        return NULL;
    }

    item->type = type;
    memcpy(item->node_id, EMPTY_NODE_ID, SELVA_NODE_ID_SIZE);
    item->node = NULL;
    if (type == ORDERED_ITEM_TYPE_TEXT && data_len > 0) {
        strxfrm_l(item->data, data, final_data_len + 1, locale);
    }
    item->data_len = final_data_len;
    item->d = d;
    item->data_obj = obj;

    return item;
}

int SelvaTraversal_FieldsContains(struct SelvaObject *fields, const char *field_name_str, size_t field_name_len) {
    void *iterator;
    const SVector *vec;

    iterator = SelvaObject_ForeachBegin(fields);
    while ((vec = SelvaObject_ForeachValue(fields, &iterator, NULL, SELVA_OBJECT_ARRAY))) {
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

int SelvaTraversal_GetSkip(enum SelvaTraversal dir) {
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
    case SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION:
        return 1;
    default:
        return 0;
    }
}

const char *SelvaTraversal_Dir2str(enum SelvaTraversal dir) {
    switch (dir) {
    case SELVA_HIERARCHY_TRAVERSAL_NONE:
        return (const char *)"none";
    case SELVA_HIERARCHY_TRAVERSAL_NODE:
        return (const char *)"node";
    case SELVA_HIERARCHY_TRAVERSAL_ARRAY:
        return (const char *)"array";
    case SELVA_HIERARCHY_TRAVERSAL_CHILDREN:
        return (const char *)"children";
    case SELVA_HIERARCHY_TRAVERSAL_PARENTS:
        return (const char *)"parents";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
        return (const char *)"bfs_ancestors";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
        return (const char *)"bfs_descendants";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
        return (const char *)"dfs_ancestors";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
        return (const char *)"dfs_descendants";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_FULL:
        return (const char *)"dfs_full";
    case SELVA_HIERARCHY_TRAVERSAL_REF:
        return (const char *)"ref";
    case SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD:
        return (const char *)"edge_field";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD:
        return (const char *)"bfs_edge_field";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION:
        return (const char *)"bfs_expression";
    case SELVA_HIERARCHY_TRAVERSAL_EXPRESSION:
        return (const char *)"expression";
    default:
        return "invalid";
    }
}
