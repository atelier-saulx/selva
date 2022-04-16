#pragma once
#ifndef SELVA_TRAVERSAL_ORDER_H
#define SELVA_TRAVERSAL_ORDER_H

struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaHierarchyNode;
struct SelvaObject;

enum TraversalOrderItemType {
    ORDER_ITEM_TYPE_EMPTY,
    ORDER_ITEM_TYPE_TEXT,
    ORDER_ITEM_TYPE_DOUBLE,
};

struct TraversalOrderItem {
    enum TraversalOrderItemType type;
    Selva_NodeId node_id;
    struct SelvaHierarchyNode *node;
    struct SelvaObject *data_obj;
    double d;
    size_t data_len;
    char data[];
};

enum SelvaResultOrder {
    SELVA_RESULT_ORDER_NONE,
    SELVA_RESULT_ORDER_ASC,
    SELVA_RESULT_ORDER_DESC,
};

int SelvaTraversal_ParseOrder(
        struct RedisModuleString **order_by_field,
        enum SelvaResultOrder *order,
        const struct RedisModuleString *txt,
        struct RedisModuleString *fld,
        struct RedisModuleString *ord);

int SelvaTraversalOrder_InitOrderResult(SVector *order_result, enum SelvaResultOrder order, ssize_t limit);

/**
 * Destroy an order_result SVector and free its items properly.
 * If SelvaTraversalOrder_CreateOrderItem() was called with a ctx then ctx this
 * function should be called with a ctx too. Alternatively the order_result
 * SVector can be declared with SVECTOR_AUTOFREE().
 */
void SelvaTraversalOrder_DestroyOrderResult(RedisModuleCtx *ctx, SVector *order_result);

/**
 * Create a new TraversalOrderItem that can be sorted.
 * @param[in] ctx if given the item will be freed when the context exits; if NULL the caller must free the item returned.
 */
struct TraversalOrderItem *SelvaTraversalOrder_CreateOrderItem(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *lang,
        struct SelvaHierarchyNode *node,
        const struct RedisModuleString *order_field);
void SelvaTraversalOrder_DestroyOrderItem(RedisModuleCtx *ctx, struct TraversalOrderItem *item);
struct TraversalOrderItem *SelvaTraversalOrder_CreateObjectBasedOrderItem(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *lang,
        struct SelvaObject *obj,
        const struct RedisModuleString *order_field);

#endif /* SELVA_TRAVERSAL_ORDER_H */
