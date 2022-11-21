/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include "jemalloc.h"
#include "util/finalizer.h"
#include "util/funmap.h"
#include "util/ptag.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_log.h"
#include "arg_parser.h"
#include "selva_db.h"
#include "hierarchy.h"
#include "selva_lang.h"
#include "selva_object.h"
#include "traversal.h"

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

int SelvaTraversal_ParseOrder(enum SelvaResultOrder *order, struct selva_string *ord) {
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
        struct selva_string **order_by_field,
        enum SelvaResultOrder *order,
        const struct selva_string *txt,
        struct selva_string *fld,
        struct selva_string *ord) {
    TO_STR(txt, fld);
    int err;

    if (strcmp("order", txt_str)) {
        return SELVA_HIERARCHY_ENOENT;
    }

    err = SelvaTraversal_ParseOrder(order, ord);
    if (err) {
        TO_STR(ord);

        SELVA_LOG(SELVA_LOGL_ERR, "Invalid order \"%.*s\": %s",
                  (int)ord_len, ord_str,
                  selva_strerror(err));
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
    const char *a_str = a->data;
    const char *b_str = b->data;

    if (a->type == ORDER_ITEM_TYPE_DOUBLE &&
        b->type == ORDER_ITEM_TYPE_DOUBLE) {
        double x = a->d;
        double y = b->d;

        if (x < y) {
            return -1;
        } else if (x > y) {
            return 1;
        }
    } else if (a->type == ORDER_ITEM_TYPE_TEXT &&
               b->type == ORDER_ITEM_TYPE_TEXT) {
        const int res = strcmp(a_str, b_str);

        if (res != 0) {
            return res;
        }
    } else if (b->type - a->type) {
        return b->type - a->type;
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

void SelvaTraversalOrder_DestroyOrderResult(struct RedisModuleCtx *ctx, SVector *order_result) {
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
static void obj_any2order_data(struct SelvaObjectAny *any, struct order_data *tmp) {

    if (any->type == SELVA_OBJECT_STRING) {
        struct selva_string *value = any->str;

        if (value) {
            tmp->data = selva_string_to_str(value, &tmp->data_len);
            tmp->type = ORDER_ITEM_TYPE_TEXT;

            if (any->user_meta == SELVA_OBJECT_META_SUBTYPE_TEXT) {
                memcpy(tmp->data_lang, any->str_lang, min(sizeof(tmp->data_lang), sizeof(any->str_lang)));
                tmp->data_lang[sizeof(tmp->data_lang) - 1] = '\0';
            }
        }
    } else if (any->type == SELVA_OBJECT_DOUBLE) {
        tmp->d = any->d;
        tmp->type = ORDER_ITEM_TYPE_DOUBLE;
    } else if (any->type == SELVA_OBJECT_LONGLONG) {
        tmp->d = (double)any->ll;
        tmp->type = ORDER_ITEM_TYPE_DOUBLE;
    }
}

static size_t calc_final_data_len(const char *data_lang, const char *data, size_t data_len, locale_t *locale_p) {
    size_t final_data_len = data_len;
    locale_t locale = 0;

    if (data_len > 0) {
        locale = SelvaLang_GetLocale(data_lang, strlen(data_lang));
        final_data_len = strxfrm_l(NULL, data, 0, locale);
    }

    *locale_p = locale;
    return final_data_len;
}

static struct TraversalOrderItem *alloc_item(struct finalizer *fin, size_t data_size) {
    const size_t item_size = sizeof(struct TraversalOrderItem) + data_size;
    struct TraversalOrderItem *item;

    item = selva_calloc(1, item_size);

    if (fin) {
        finalizer_add(fin, item, selva_free);
    }

    return item;
}

static struct TraversalOrderItem *create_item(struct finalizer *fin, const struct order_data * restrict tmp, enum TraversalOrderItemPtype order_ptype, void *p) {
    locale_t locale = 0;
    size_t data_size = 0;
    struct TraversalOrderItem *item;

    if (tmp->type == ORDER_ITEM_TYPE_TEXT) {
        data_size = calc_final_data_len(tmp->data_lang, tmp->data, tmp->data_len, &locale) + 1;
    }

    item = alloc_item(fin, data_size);
    item->type = tmp->type;

    if (tmp->type == ORDER_ITEM_TYPE_TEXT) {
        item->d = nan("");
        if (tmp->data_len > 0) {
            strxfrm_l(item->data, tmp->data, data_size, locale);
        }
    } else if (tmp->type == ORDER_ITEM_TYPE_DOUBLE) {
        item->d = tmp->d;
    }

    switch (order_ptype) {
    case TRAVERSAL_ORDER_ITEM_PTYPE_NODE:
        item->tagp = PTAG(p, TRAVERSAL_ORDER_ITEM_PTYPE_NODE);
        SelvaHierarchy_GetNodeId(item->node_id, p);
        break;
    case TRAVERSAL_ORDER_ITEM_PTYPE_OBJ:
        item->tagp = PTAG(p, TRAVERSAL_ORDER_ITEM_PTYPE_OBJ);
        memcpy(item->node_id, EMPTY_NODE_ID, SELVA_NODE_ID_SIZE);
        break;
    default:
        SelvaTraversalOrder_DestroyOrderItem(NULL, item);
        return NULL;
    }

    return item;
}

struct TraversalOrderItem *SelvaTraversalOrder_CreateNodeOrderItem(
        struct finalizer *fin,
        struct selva_string *lang,
        struct SelvaHierarchyNode *node,
        const struct selva_string *order_field) {
    int err;
    struct SelvaObjectAny any;
    struct order_data tmp = {
        .type = ORDER_ITEM_TYPE_EMPTY,
    };

    err = SelvaObject_GetAnyLang(SelvaHierarchy_GetNodeObject(node), lang, order_field, &any);
    if (!err) {
        obj_any2order_data(&any, &tmp);
    } else if (err != SELVA_ENOENT) {
        return NULL;
    }

    return create_item(fin, &tmp, TRAVERSAL_ORDER_ITEM_PTYPE_NODE, node);
}

struct TraversalOrderItem *SelvaTraversalOrder_CreateAnyNodeOrderItem(
        struct finalizer *fin,
        struct SelvaHierarchyNode *node,
        struct SelvaObjectAny *any) {
    struct order_data tmp = {
        .type = ORDER_ITEM_TYPE_EMPTY,
    };

    obj_any2order_data(any, &tmp);
    return create_item(fin, &tmp, TRAVERSAL_ORDER_ITEM_PTYPE_NODE, node);
}

struct TraversalOrderItem *SelvaTraversalOrder_CreateObjectOrderItem(
        struct finalizer *fin,
        struct selva_string *lang,
        struct SelvaObject *obj,
        const struct selva_string *order_field) {
    int err;
    struct SelvaObjectAny any;
    struct order_data tmp = {
        .type = ORDER_ITEM_TYPE_EMPTY,
    };

    err = SelvaObject_GetAnyLang(obj, lang, order_field, &any);
    if (!err) {
        obj_any2order_data(&any, &tmp);
    } else if (err != SELVA_ENOENT) {
        return NULL;
    }

    return create_item(fin, &tmp, TRAVERSAL_ORDER_ITEM_PTYPE_OBJ, obj);
}

void SelvaTraversalOrder_DestroyOrderItem(struct RedisModuleCtx *ctx, struct TraversalOrderItem *item) {
    if (!ctx) {
        selva_free(item);
    }
    /* Otherwise it's allocated from the pool. */
}
