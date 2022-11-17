/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef SELVA_TRAVERSAL_H
#define SELVA_TRAVERSAL_H

#include "util/svector.h"

struct FindCommand_Args;
struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaHierarchy;
struct SelvaHierarchyNode;
struct SelvaObjectAny;

/**
 * Hierarchy traversal order.
 * Recognized by SelvaHierarchy_Traverse().
 */
enum SelvaTraversal {
    SELVA_HIERARCHY_TRAVERSAL_NONE =            0x0000, /*!< Do nothing. */
    SELVA_HIERARCHY_TRAVERSAL_NODE =            0x0001, /*!< Visit just the given node. */
    SELVA_HIERARCHY_TRAVERSAL_ARRAY =           0x0002, /*!< Traverse an array. */
    SELVA_HIERARCHY_TRAVERSAL_SET =             0x0004, /*!< Traverse an array. */
    SELVA_HIERARCHY_TRAVERSAL_REF =             0x0008, /*!< Visit nodes pointed by a string ref field. */
    SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD =      0x0010, /*!< Visit nodes pointed by an edge field. */
    SELVA_HIERARCHY_TRAVERSAL_CHILDREN =        0x0020, /*!< Visit children of the given node. */
    SELVA_HIERARCHY_TRAVERSAL_PARENTS =         0x0040, /*!< Visit parents of the given node. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS =   0x0080, /*!< Visit ancestors of the given node using BFS. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS = 0x0100, /*!< Visit descendants of the given node using BFS. */
    SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS =   0x0200, /*!< Visit ancestors of the given node using DFS. */
    SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS = 0x0400, /*!< Visit descendants of the given node using DFS. */
    SELVA_HIERARCHY_TRAVERSAL_DFS_FULL =        0x0800, /*!< Full DFS traversal of the whole hierarchy. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD =  0x1000, /*!< Traverse an edge field according to its constraints using BFS. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION =  0x2000, /*!< Traverse with an expression returning a set of field names. */
    SELVA_HIERARCHY_TRAVERSAL_EXPRESSION =      0x4000, /*!< Visit fields with an expression returning a set of field names. */
};

enum SelvaMergeStrategy {
    MERGE_STRATEGY_NONE = 0, /* No merge. */
    MERGE_STRATEGY_ALL,
    MERGE_STRATEGY_NAMED,
    MERGE_STRATEGY_DEEP,
};

/**
 * Traversal result order.
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

/**
 * Traversal order item type.
 * As a TraversalOrderItem can contain data of different types, this enum
 * encodes the type to help finding the right way to compare two items.
 */
enum TraversalOrderItemType {
    ORDER_ITEM_TYPE_EMPTY = 0,
    ORDER_ITEM_TYPE_TEXT,
    ORDER_ITEM_TYPE_DOUBLE,
};

/**
 * Tag type for tagp in struct TraversalOrderItem.
 */
enum TraversalOrderItemPtype {
    /**
     * A pointer to a node.
     */
    TRAVERSAL_ORDER_ITEM_PTYPE_NODE = 1,
    /**
     * A pointer to a SelvaObject.
     */
    TRAVERSAL_ORDER_ITEM_PTYPE_OBJ,
};

/**
 * Traversal order item.
 * These are usually stored in an SVector initialized by
 * SelvaTraversalOrder_InitOrderResult().
 */
struct TraversalOrderItem {
    /**
     * Value type of this ordered item.
     */
    enum TraversalOrderItemType type;
    /**
     * Associated NodeId of this item.
     */
    Selva_NodeId node_id;
    /**
     * A pointer tagged with TraversalOrderItemPtype.
     */
    void *tagp;
    /**
     * Double value.
     */
    double d;
    /**
     * Sortable data for ORDER_ITEM_TYPE_TEXT.
     */
    char data[];
};

struct SelvaNodeSendParam {
    /*
     * Order-by information is needed if the sorting is made in the
     * postprocessing step, i.e. when the args->result SVector isn't
     * sorted.
     */
    enum SelvaResultOrder order; /*!< Result order. */
    const struct RedisModuleString *order_field; /*!< Order by field name; Otherwise NULL. */

    /**
     * Merge strategy.
     * A merge is executed if this field is set to other than MERGE_STRATEGY_NONE.
     */
    enum SelvaMergeStrategy merge_strategy;
    struct RedisModuleString *merge_path;

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

    /**
     * Field names expression context for `fields_expression`.
     */
    struct rpn_ctx *fields_rpn_ctx;

    /**
     * Field names expression.
     * Another way to select which fields should be returned to the client is
     * using an RPN expression that returns a set on field names.
     */
    struct rpn_expression *fields_expression;

    /**
     * Inherit fields using ancestors traversal.
     * Optional, can be set to NULL.
     * The last element shall be NULL.
     */
    struct RedisModuleString **inherit_fields;

    /**
     * Fields that should be excluded when `fields` contains a wildcard.
     * The list should delimit the excluded fields in the following way:
     * ```
     * \0field1\0field2\0
     * ```
     */
    struct RedisModuleString *excluded_fields;
};

typedef int (*SelvaFind_ProcessNode)(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct FindCommand_Args *args,
        struct SelvaHierarchyNode *node);
typedef int (*SelvaFind_ProcessObject)(
        struct RedisModuleCtx *ctx,
        struct FindCommand_Args *args,
        struct SelvaObject *obj);

typedef void (*SelvaFind_Postprocess)(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct RedisModuleString *lang,
        ssize_t offset,
        ssize_t limit,
        struct SelvaNodeSendParam *args,
        SVector *result);

struct FindCommand_Args {
    struct RedisModuleString *lang;

    ssize_t *nr_nodes; /*!< Number of nodes in the result. */
    ssize_t offset; /*!< Start from nth node. */
    ssize_t *limit; /*!< Limit the number of result. */

    struct rpn_ctx *rpn_ctx;
    const struct rpn_expression *filter;

    struct SelvaNodeSendParam send_param;
    size_t *merge_nr_fields;

#if 0
    enum SelvaResultOrder order; /*!< Result order. */
#endif
    SVector *result; /*!< Results of the find for postprocessing. Wrapped in TraversalOrderItem structs if sorting is requested. */

    struct Selva_SubscriptionMarker *marker; /*!< Used by FindInSub. */

    /* Accounting */
    size_t acc_take; /*!< Numer of nodes selected during the traversal. */
    size_t acc_tot; /*!< Total number of nodes visited during the traversal. */

    union {
        SelvaFind_ProcessNode process_node;
        SelvaFind_ProcessObject process_obj;
    };
};

/**
 * Called for the first node in the traversal.
 * This is typically the node that was given as an argument to a traversal function.
 * @param node a pointer to the node.
 * @param arg a pointer to head_arg give in SelvaHierarchyCallback structure.
 */
typedef int (*SelvaHierarchyHeadCallback)(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg);

/**
 * Called for each node found during a traversal.
 * @param node a pointer to the node.
 * @param arg a pointer to node_arg give in SelvaHierarchyCallback structure.
 * @returns 0 to continue the traversal; 1 to interrupt the traversal.
 */
typedef int (*SelvaHierarchyNodeCallback)(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg);

/**
 * Traversal metadata for child/adjacent nodes.
 */
struct SelvaHierarchyTraversalMetadata {
    const char *origin_field_str;
    size_t origin_field_len;
    struct SelvaHierarchyNode *origin_node;
};

/**
 * Called for each adjacent node during a traversal.
 * @param node a pointer to the node.
 * @param arg a pointer to child_arg give in SelvaHierarchyCallback structure.
 */
typedef void (*SelvaHierarchyChildCallback)(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const struct SelvaHierarchyTraversalMetadata *metadata,
        struct SelvaHierarchyNode *child,
        void *arg);

int SelvaTraversal_ParseDir2(enum SelvaTraversal *dir, const struct RedisModuleString *arg);
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

int SelvaTraversal_FieldsContains(struct SelvaObject *fields, const char *field_name_str, size_t field_name_len);
int SelvaTraversal_GetSkip(enum SelvaTraversal dir);
const char *SelvaTraversal_Dir2str(enum SelvaTraversal dir);

/**
 * Init an SVector for storing TraversalOrderItems.
 * @param order_result is a pointer to the SVector to be initialized.
 * @param order is the order requested.
 * @param limit is the expected length for the final SVector. Generally this can be the same as limit size of the response. 0 = auto.
 */
void SelvaTraversalOrder_InitOrderResult(SVector *order_result, enum SelvaResultOrder order, ssize_t limit);

/**
 * Destroy an order_result SVector and free its items properly.
 * If SelvaTraversalOrder_Create*OrderItem() was called with a ctx then ctx this
 * function should be called with a ctx too. Alternatively the order_result
 * SVector can be declared with SVECTOR_AUTOFREE().
 */
void SelvaTraversalOrder_DestroyOrderResult(struct RedisModuleCtx *ctx, SVector *order_result);

/**
 * Create a new node based TraversalOrderItem that can be sorted.
 * @param[in] ctx if given the item will be freed when the context exits; if NULL the caller must free the item returned.
 * @returns Returns a TraversalOrderItem if succeed; Otherwise a NULL pointer is returned.
 */
struct TraversalOrderItem *SelvaTraversalOrder_CreateNodeOrderItem(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *lang,
        struct SelvaHierarchyNode *node,
        const struct RedisModuleString *order_field);

/**
 * Create a new node based TraversalOrderItem that can be sorted with user defined value.
 * @param[in] ctx if given the item will be freed when the context exits; if NULL the caller must free the item returned.
 * @returns Returns a TraversalOrderItem if succeed; Otherwise a NULL pointer is returned.
 */
struct TraversalOrderItem *SelvaTraversalOrder_CreateAnyNodeOrderItem(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchyNode *node,
        struct SelvaObjectAny *any);

/**
 * Create a new SelvaObject based TraversalOrderItem that can be sorted.
 * This function can be used to determine an order for several SelvaObjects.
 * @param lang is the language for text fields.
 * @param order_field is a field on obj.
 * @returns Returns a TraversalOrderItem if succeed; Otherwise a NULL pointer is returned.
 */
struct TraversalOrderItem *SelvaTraversalOrder_CreateObjectOrderItem(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *lang,
        struct SelvaObject *obj,
        const struct RedisModuleString *order_field);

/**
 * Destroy TraversalOrderItem created by SelvaTraversalOrder_Create*OrderItem().
 */
void SelvaTraversalOrder_DestroyOrderItem(struct RedisModuleCtx *ctx, struct TraversalOrderItem *item);

#endif /* SELVA_TRAVERSAL_H */
