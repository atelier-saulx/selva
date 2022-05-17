#pragma once
#ifndef SELVA_TRAVERSAL_ORDER_H
#define SELVA_TRAVERSAL_ORDER_H

struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaHierarchyNode;
struct SelvaObject;

/**
 * Traversal order item type.
 * As a TraversalOrderItem can contain data of different types, this enum
 * encodes the type to help finding the right way to compare two items.
 */
enum TraversalOrderItemType {
    ORDER_ITEM_TYPE_EMPTY,
    ORDER_ITEM_TYPE_TEXT,
    ORDER_ITEM_TYPE_DOUBLE,
};

/**
 * Traversal order item.
 * These are usually stored in an SVector initialized by
 * SelvaTraversalOrder_InitOrderResult().
 */
struct TraversalOrderItem {
    enum TraversalOrderItemType type;
    Selva_NodeId node_id;
    struct SelvaHierarchyNode *node;
    struct SelvaObject *data_obj;
    double d;
    size_t data_len;
    char data[];
};

/**
 * Result order.
 */
enum SelvaResultOrder {
    /**
     * Result is not ordered by any field but can be usually expected to have a
     * deterministic order.
     */
    SELVA_RESULT_ORDER_NONE,
    /**
     * Ascending order.
     */
    SELVA_RESULT_ORDER_ASC,
    /**
     * Descending order.
     */
    SELVA_RESULT_ORDER_DESC,
};

int SelvaTraversal_ParseOrder(enum SelvaResultOrder *order, struct RedisModuleString *ord);

/**
 * Parse an `order` argument in a command call.
 */
int SelvaTraversal_ParseOrderArg(
        struct RedisModuleString **order_by_field,
        enum SelvaResultOrder *order,
        const struct RedisModuleString *txt,
        struct RedisModuleString *fld,
        struct RedisModuleString *ord);

/**
 * Init an SVector for storing TraversalOrderItems.
 * @param order_result is a pointer to the SVector to be initialized.
 * @param order is the order requested.
 * @param limit is the expected length for the final SVector. Generally this can be the same as limit size of the response. 0 = auto.
 */
int SelvaTraversalOrder_InitOrderResult(SVector *order_result, enum SelvaResultOrder order, ssize_t limit);

/**
 * Destroy an order_result SVector and free its items properly.
 * If SelvaTraversalOrder_CreateOrderItem() was called with a ctx then ctx this
 * function should be called with a ctx too. Alternatively the order_result
 * SVector can be declared with SVECTOR_AUTOFREE().
 */
void SelvaTraversalOrder_DestroyOrderResult(struct RedisModuleCtx *ctx, SVector *order_result);

/**
 * Create a new TraversalOrderItem that can be sorted.
 * @param[in] ctx if given the item will be freed when the context exits; if NULL the caller must free the item returned.
 * @returns Returns a TraversalOrderItem if succeed; Otherwise a NULL pointer is returned.
 */
struct TraversalOrderItem *SelvaTraversalOrder_CreateOrderItem(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *lang,
        struct SelvaHierarchyNode *node,
        const struct RedisModuleString *order_field);
<<<<<<< HEAD
void SelvaTraversalOrder_DestroyOrderItem(struct RedisModuleCtx *ctx, struct TraversalOrderItem *item);
=======

/**
 * Destroy TraversalOrderItem created by SelvaTraversalOrder_CreateOrderItem().
 */
void SelvaTraversalOrder_DestroyOrderItem(RedisModuleCtx *ctx, struct TraversalOrderItem *item);

/**
 * Create a TraversalOrderItem that points to a SelvaObject.
 * This function can be used to determine an order for several SelvaObjects.
 * @param lang is the language for text fields.
 * @param order_field is a field on obj.
 * @returns Returns a TraversalOrderItem if succeed; Otherwise a NULL pointer is returned.
 */
>>>>>>> dc36fb90 (Add more comments regarding traversal_order)
struct TraversalOrderItem *SelvaTraversalOrder_CreateObjectBasedOrderItem(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *lang,
        struct SelvaObject *obj,
        const struct RedisModuleString *order_field);

#endif /* SELVA_TRAVERSAL_ORDER_H */
