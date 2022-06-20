#include "redismodule.h"
#include "cdefs.h"
#include "funmap.h"
#include "selva.h"
#include "errors.h"
#include "hierarchy.h"
#include "selva_lang.h"
#include "selva_object.h"
#include "traversal_order.h"

/**
 * Used internally to pass the data needed for creating order items.
 */
struct order_data {
    double d;
    const char *data;
    size_t data_len;
    char data_lang[LANG_MAX];
    enum TraversalOrderItemType type;
};

typedef int (*orderFunc)(const void ** restrict a_raw, const void ** restrict b_raw);

static const struct SelvaArgParser_EnumType order_types[] = {
    {
        .name = "none",
        .id = SELVA_RESULT_ORDER_NONE,
    },
    {
        .name = "asc",
        .id = SELVA_RESULT_ORDER_ASC,
    },
    {
        .name = "desc",
        .id = SELVA_RESULT_ORDER_DESC,
    },
    /* Must be last. */
    {
        .name = NULL,
        .id = 0,
    }
};

int SelvaTraversal_ParseOrder(enum SelvaResultOrder *order, struct RedisModuleString *ord) {
    int res;

    res = SelvaArgParser_Enum(order_types, ord);
    if (res < 0) {
        *order = SELVA_RESULT_ORDER_NONE;
        return res;
    }

    *order = (enum SelvaResultOrder)res;
    return 0;
}

int SelvaTraversal_ParseOrderArg(
        RedisModuleString **order_by_field,
        enum SelvaResultOrder *order,
        const RedisModuleString *txt,
        RedisModuleString *fld,
        RedisModuleString *ord) {
    TO_STR(txt, fld);
    int err;

    if (strcmp("order", txt_str)) {
        return SELVA_HIERARCHY_ENOENT;
    }

    err = SelvaTraversal_ParseOrder(order, ord);
    if (err) {
        TO_STR(ord);

        fprintf(stderr, "%s:%d: Invalid order \"%.*s\": %s\n",
                __FILE__, __LINE__,
                (int)ord_len, ord_str,
                getSelvaErrorStr(err));
        return SELVA_HIERARCHY_EINVAL;
    }

    if (fld_len == 0 || fld_str[0] == '\0') {
        *order = SELVA_RESULT_ORDER_NONE;
        *order_by_field = NULL;
    } else {
        *order_by_field = *order == SELVA_RESULT_ORDER_NONE ? NULL : fld;
    }

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

void SelvaTraversalOrder_InitOrderResult(SVector *order_result, enum SelvaResultOrder order, ssize_t limit) {
    const size_t initial_len = (limit > 0) ? limit : HIERARCHY_EXPECTED_RESP_LEN;

    SVector_Init(order_result, initial_len, SelvaTraversal_GetOrderFunc(order));
}

void SelvaTraversalOrder_DestroyOrderResult(RedisModuleCtx *ctx, SVector *order_result) {
    struct SVectorIterator it;
    struct TraversalOrderItem *item;

    SVector_ForeachBegin(&it, order_result);
    while ((item = SVector_Foreach(&it))) {
        SelvaTraversalOrder_DestroyOrderItem(ctx, item);
    }


    SVector_Destroy(order_result);
}

/**
 * Parse data for creating an order item.
 * For most part all errors are ignored and we just won't be able determine a
 * satisfying order.
 */
static int obj2order_data(RedisModuleString *lang, struct SelvaObject *obj, const RedisModuleString *order_field, struct order_data *tmp) {
    enum SelvaObjectType obj_type;

    obj_type = SelvaObject_GetType(obj, order_field);
    if (obj_type == SELVA_OBJECT_STRING) {
        RedisModuleString *value = NULL;
        int err;

        err = SelvaObject_GetString(obj, order_field, &value);
        if (!err && value) {
            tmp->data = RedisModule_StringPtrLen(value, &tmp->data_len);
            tmp->type = ORDER_ITEM_TYPE_TEXT;
        }
    } else if (obj_type == SELVA_OBJECT_OBJECT) {
        SelvaObjectMeta_t meta;

        SelvaObject_GetUserMeta(obj, order_field, &meta);
        if (meta == SELVA_OBJECT_META_SUBTYPE_TEXT) {
            struct SelvaObject *text_obj;
            TO_STR(lang);
            int err;

            if (lang_len == 0) {
                return SELVA_EINVAL;
            }

            err = SelvaObject_GetObject(obj, order_field, &text_obj);
            if (err) {
                return err;
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
                        strncpy(tmp->data_lang, token, sizeof(tmp->data_lang) - 1);
                        tmp->data_lang[sizeof(tmp->data_lang) - 1] = '\0';
                        tmp->data = raw_value_str;
                        tmp->data_len = raw_value_len;
                        tmp->type = ORDER_ITEM_TYPE_TEXT;
                        break;
                    }
                }
            }
        }
    } else if (obj_type == SELVA_OBJECT_DOUBLE) {
        int err;

        err = SelvaObject_GetDouble(obj, order_field, &tmp->d);
        if (!err) {
            tmp->type = ORDER_ITEM_TYPE_DOUBLE;
        }
    } else if (obj_type == SELVA_OBJECT_LONGLONG) {
        long long v;
        int err;

        err = SelvaObject_GetLongLong(obj, order_field, &v);
        if (!err) {
            tmp->d = (double)v;
            tmp->type = ORDER_ITEM_TYPE_DOUBLE;
        }
    }

    return 0;
}

static size_t calc_final_data_len(enum TraversalOrderItemType type, const char *data_lang, const char *data, size_t data_len, locale_t *locale_p) {
    size_t final_data_len = data_len;
    locale_t locale = 0;

    if (type == ORDER_ITEM_TYPE_TEXT && data_len > 0) {
        locale = SelvaLang_GetLocale(data_lang, strlen(data_lang));
        final_data_len = strxfrm_l(NULL, data, 0, locale);
    }

    *locale_p = locale;
    return final_data_len;
}

static struct TraversalOrderItem *alloc_item(RedisModuleCtx *ctx, size_t final_data_len) {
    const size_t item_size = sizeof(struct TraversalOrderItem) + final_data_len + 1;

    if (ctx) {
        return RedisModule_PoolAlloc(ctx, item_size);
    } else {
        return RedisModule_Calloc(1, item_size);
    }
}

struct TraversalOrderItem *SelvaTraversalOrder_CreateOrderItem(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        struct SelvaHierarchyNode *node,
        const RedisModuleString *order_field) {
    Selva_NodeId nodeId;
    struct SelvaObject *obj;
    struct order_data tmp = {
        .d = 0.0,
        .data = NULL,
        .data_len = 0,
        .type = ORDER_ITEM_TYPE_EMPTY,
    };

    memset(tmp.data_lang, '\0', sizeof(tmp.data_lang));
    SelvaHierarchy_GetNodeId(nodeId, node);
    obj = SelvaHierarchy_GetNodeObject(node);

    if (obj2order_data(lang, obj, order_field, &tmp)) {
        return NULL;
    }

    locale_t locale;
    const size_t final_data_len = calc_final_data_len(tmp.type, tmp.data_lang, tmp.data, tmp.data_len, &locale);
    struct TraversalOrderItem *item = alloc_item(ctx, final_data_len);
    if (!item) {
        return NULL;
    }

    item->type = tmp.type;
    memcpy(item->node_id, nodeId, SELVA_NODE_ID_SIZE);
    item->node = node;
    if (tmp.type == ORDER_ITEM_TYPE_TEXT && tmp.data_len > 0) {
        strxfrm_l(item->data, tmp.data, final_data_len + 1, locale);
    }
    item->data_len = final_data_len;
    item->d = tmp.d;

    return item;
}

void SelvaTraversalOrder_DestroyOrderItem(RedisModuleCtx *ctx, struct TraversalOrderItem *item) {
    if (!ctx) {
        RedisModule_Free(item);
    }
}

struct TraversalOrderItem *SelvaTraversalOrder_CreateObjectBasedOrderItem(
        RedisModuleCtx *ctx,
        RedisModuleString *lang,
        struct SelvaObject *obj,
        const RedisModuleString *order_field) {
    struct order_data tmp = {
        .d = 0.0,
        .data = NULL,
        .data_len = 0,
        .type = ORDER_ITEM_TYPE_EMPTY,
    };

    memset(tmp.data_lang, '\0', sizeof(tmp.data_lang));

    if (obj2order_data(lang, obj, order_field, &tmp)) {
        return NULL;
    }

    locale_t locale;
    const size_t final_data_len = calc_final_data_len(tmp.type, tmp.data_lang, tmp.data, tmp.data_len, &locale);
    struct TraversalOrderItem *item = alloc_item(ctx, final_data_len);
    if (!item) {
        return NULL;
    }

    item->type = tmp.type;
    memcpy(item->node_id, EMPTY_NODE_ID, SELVA_NODE_ID_SIZE);
    item->node = NULL;
    if (tmp.type == ORDER_ITEM_TYPE_TEXT && tmp.data_len > 0) {
        strxfrm_l(item->data, tmp.data, final_data_len + 1, locale);
    }
    item->data_len = final_data_len;
    item->d = tmp.d;
    item->data_obj = obj;

    return item;
}
