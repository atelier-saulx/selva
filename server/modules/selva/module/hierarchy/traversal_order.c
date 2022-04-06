#include "redismodule.h"
#include "cdefs.h"
#include "funmap.h"
#include "selva.h"
#include "errors.h"
#include "hierarchy.h"
#include "selva_lang.h"
#include "selva_object.h"
#include "traversal_order.h"

typedef int (*orderFunc)(const void ** restrict a_raw, const void ** restrict b_raw);

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
        tmpOrder = SELVA_RESULT_ORDER_ASC;
    } else if (ord_str[0] == 'd' && !strcmp("desc", ord_str)) {
        tmpOrder = SELVA_RESULT_ORDER_DESC;
    } else {
einval:
        fprintf(stderr, "%s:%d: Invalid order \"%.*s\"\n",
                __FILE__, __LINE__,
                (int)ord_len, ord_str);
        return SELVA_HIERARCHY_EINVAL;
    }

    if (fld_len == 0 || fld_str[0] == '\0') {
        tmpOrder = SELVA_RESULT_ORDER_NONE;
        *order_by_field = NULL;
    } else {
        *order_by_field = fld;
    }

    *order = tmpOrder;

    return 0;
}

static int SelvaTraversalOrder_CompareNone(const void ** restrict a_raw __unused, const void ** restrict b_raw __unused) {
    return 0;
}

static int SelvaTraversalOrder_CompareAsc(const void ** restrict a_raw, const void ** restrict b_raw) {
    const struct TraversalOrderItem *a = *(const struct TraversalOrderItem **)a_raw;
    const struct TraversalOrderItem *b = *(const struct TraversalOrderItem **)b_raw;
    const char *aStr = a->data;
    const char *bStr = b->data;

    if (a->type == ORDER_ITEM_TYPE_DOUBLE &&
        b->type == ORDER_ITEM_TYPE_DOUBLE) {
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
static int SelvaTraversalOrder_CompareDesc(const void ** restrict a_raw, const void ** restrict b_raw) {
    return SelvaTraversalOrder_CompareAsc(b_raw, a_raw);
}

static orderFunc order_functions[] = {
    [SELVA_RESULT_ORDER_NONE] = SelvaTraversalOrder_CompareNone,
    [SELVA_RESULT_ORDER_ASC] = SelvaTraversalOrder_CompareAsc,
    [SELVA_RESULT_ORDER_DESC] = SelvaTraversalOrder_CompareDesc,
};

GENERATE_STATIC_FUNMAP(SelvaTraversal_GetOrderFunc, order_functions, enum SelvaResultOrder, SELVA_RESULT_ORDER_NONE);

int SelvaTraversalOrder_InitOrderResult(SVector *order_result, enum SelvaResultOrder order, ssize_t limit) {
    const size_t initial_len = (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN;

    return SVector_Init(order_result, initial_len, SelvaTraversal_GetOrderFunc(order)) ? 0 : SELVA_ENOMEM;
}

struct TraversalOrderItem *SelvaTraversalOrder_CreateOrderItem(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        struct SelvaHierarchyNode *node,
        const RedisModuleString *order_field) {
    Selva_NodeId nodeId;
    struct TraversalOrderItem *item = NULL;
    double d = 0.0;
    char data_lang[LANG_MAX];
    const char *data = NULL;
    size_t data_len = 0;
    enum TraversalOrderItemType type = ORDER_ITEM_TYPE_EMPTY;

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
            type = ORDER_ITEM_TYPE_TEXT;
        }
    } else if (obj_type == SELVA_OBJECT_OBJECT) {
        SelvaObjectMeta_t meta;
        SelvaObject_GetUserMeta(obj, order_field, &meta);

        if (meta == SELVA_OBJECT_META_SUBTYPE_TEXT) {
            /* TODO What if lang is NULL. */
            TO_STR(lang);
            struct SelvaObject *text_obj;
            int text_err;

            if (!lang_len) {
                return NULL;
            }

            text_err = SelvaObject_GetObject(obj, order_field, &text_obj);
            if (text_err) {
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
                        type = ORDER_ITEM_TYPE_TEXT;
                        break;
                    }
                }
            }
        }
    } else if (obj_type == SELVA_OBJECT_DOUBLE) {
        err = SelvaObject_GetDouble(obj, order_field, &d);
        if (!err) {
            type = ORDER_ITEM_TYPE_DOUBLE;
        }
    } else if (obj_type == SELVA_OBJECT_LONGLONG) {
        long long v;

        err = SelvaObject_GetLongLong(obj, order_field, &v);
        if (!err) {
            d = (double)v;
            type = ORDER_ITEM_TYPE_DOUBLE;
        }
    }

    size_t final_data_len = data_len;
    locale_t locale = 0;

    if (type == ORDER_ITEM_TYPE_TEXT && data_len > 0) {
        locale = SelvaLang_GetLocale(data_lang, strlen(data_lang));
        final_data_len = strxfrm_l(NULL, data, 0, locale);
    }

    const size_t item_size = sizeof(struct TraversalOrderItem) + final_data_len + 1;
    if (ctx) {
        item = RedisModule_PoolAlloc(ctx, item_size);
    } else {
        item = RedisModule_Calloc(1, item_size);
    }
    if (!item) {
        return NULL;
    }

    item->type = type;
    memcpy(item->node_id, nodeId, SELVA_NODE_ID_SIZE);
    item->node = node;
    if (type == ORDER_ITEM_TYPE_TEXT && data_len > 0) {
        strxfrm_l(item->data, data, final_data_len + 1, locale);
    }
    item->data_len = final_data_len;
    item->d = d;

    return item;
}

struct TraversalOrderItem *SelvaTraversalOrder_CreateObjectBasedOrderItem(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        struct SelvaObject *obj,
        const RedisModuleString *order_field) {
    struct TraversalOrderItem *item = NULL;
    double d = 0.0;
    const char *data = NULL;
    size_t data_len = 0;
    char data_lang[LANG_MAX];
    enum SelvaObjectType obj_type;
    enum TraversalOrderItemType type = ORDER_ITEM_TYPE_EMPTY;

    memset(data_lang, '\0', sizeof(data_lang));

    obj_type = SelvaObject_GetType(obj, order_field);
    if (obj_type == SELVA_OBJECT_STRING) {
        RedisModuleString *value = NULL;
        int err;

        err = SelvaObject_GetString(obj, order_field, &value);
        if (!err && value) {
            data = RedisModule_StringPtrLen(value, &data_len);
            type = ORDER_ITEM_TYPE_TEXT;
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
                        type = ORDER_ITEM_TYPE_TEXT;
                        break;
                    }
                }
            }
        }
    } else if (obj_type == SELVA_OBJECT_DOUBLE) {
        int err;

        err = SelvaObject_GetDouble(obj, order_field, &d);
        if (!err) {
            type = ORDER_ITEM_TYPE_DOUBLE;
        }
    } else if (obj_type == SELVA_OBJECT_LONGLONG) {
        long long v;
        int err;

        err = SelvaObject_GetLongLong(obj, order_field, &v);
        if (!err) {
            d = (double)v;
            type = ORDER_ITEM_TYPE_DOUBLE;
        }
    }

    size_t final_data_len = data_len;
    locale_t locale = 0;

    if (type == ORDER_ITEM_TYPE_TEXT && data_len > 0) {
        locale = SelvaLang_GetLocale(data_lang, strlen(data_lang));
        final_data_len = strxfrm_l(NULL, data, 0, locale);
    }

    const size_t item_size = sizeof(struct TraversalOrderItem) + final_data_len + 1;
    if (ctx) {
        item = RedisModule_PoolAlloc(ctx, item_size);
    } else {
        item = RedisModule_Calloc(1, item_size);
    }
    if (!item) {
        return NULL;
    }

    item->type = type;
    memcpy(item->node_id, EMPTY_NODE_ID, SELVA_NODE_ID_SIZE);
    item->node = NULL;
    if (type == ORDER_ITEM_TYPE_TEXT && data_len > 0) {
        strxfrm_l(item->data, data, final_data_len + 1, locale);
    }
    item->data_len = final_data_len;
    item->d = d;
    item->data_obj = obj;

    return item;
}

