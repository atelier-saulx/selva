#pragma once
#ifndef SELVA_TRAVERSAL
#define SELVA_TRAVERSAL

#include "svector.h"
#include "arg_parser.h"

enum SelvaTraversalAlgo {
    HIERARCHY_BFS,
    HIERARCHY_DFS,
};

/**
 * Hierarchy traversal order.
 * Recognized by SelvaModify_TraverseHierarchy().
 */
enum SelvaTraversal {
    SELVA_HIERARCHY_TRAVERSAL_NONE =            0x0000, /*!< Do nothing. */
    SELVA_HIERARCHY_TRAVERSAL_NODE =            0x0001, /*!< Visit just the given node. */
    SELVA_HIERARCHY_TRAVERSAL_ARRAY =           0x0002, /*!< Traverse an array. */
    SELVA_HIERARCHY_TRAVERSAL_REF =             0x0004, /*!< Visit nodes pointed by a string ref field. */
    SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD =      0x0008, /*!< Visit nodes pointed by an edge field. */
    SELVA_HIERARCHY_TRAVERSAL_CHILDREN =        0x0010, /*!< Visit children of the given node. */
    SELVA_HIERARCHY_TRAVERSAL_PARENTS =         0x0020, /*!< Visit parents of the given node. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS =   0x0040, /*!< Visit ancestors of the given node using BFS. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS = 0x0080, /*!< Visit descendants of the given node using BFS. */
    SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS =   0x0100, /*!< Visit ancestors of the given node using DFS. */
    SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS = 0x0200, /*!< Visit descendants of the given node using DFS. */
    SELVA_HIERARCHY_TRAVERSAL_DFS_FULL =        0x0400, /*!< Full DFS traversal of the whole hierarchy. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD =  0x0800, /*!< Traverse an edge field according to its constraints using BFS. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION =  0x1000, /*!< Traverse with an expression returning a set of field names. */
    SELVA_HIERARCHY_TRAVERSAL_EXPRESSION =      0x2000, /*!< Visit fields with an expression returning a set of field names. */
};

enum SelvaMergeStrategy {
    MERGE_STRATEGY_NONE = 0, /* No merge. */
    MERGE_STRATEGY_ALL,
    MERGE_STRATEGY_NAMED,
    MERGE_STRATEGY_DEEP,
};

enum TraversalOrderedItemType {
    ORDERED_ITEM_TYPE_EMPTY,
    ORDERED_ITEM_TYPE_TEXT,
    ORDERED_ITEM_TYPE_DOUBLE,
};

struct TraversalOrderedItem {
    enum TraversalOrderedItemType type;
    Selva_NodeId node_id;
    struct SelvaHierarchyNode *node;
    struct SelvaObject *data_obj;
    double d;
    size_t data_len;
    char data[];
};

enum SelvaResultOrder {
    HIERARCHY_RESULT_ORDER_NONE,
    HIERARCHY_RESULT_ORDER_ASC,
    HIERARCHY_RESULT_ORDER_DESC,
};

typedef int (*orderFunc)(const void ** restrict a_raw, const void ** restrict b_raw);

struct RedisModuleString;
struct RedisModuleCtx;
struct SelvaHierarchy;
struct SelvaHierarchyNode;

extern const struct SelvaArgParser_EnumType merge_types[3];

struct FindCommand_Args {
    struct RedisModuleCtx *ctx;
    struct RedisModuleString *lang;
    struct SelvaHierarchy *hierarchy;

    ssize_t *nr_nodes; /*!< Number of nodes in the result. */
    ssize_t offset; /*!< Start from nth node. */
    ssize_t *limit; /*!< Limit the number of result. */

    struct rpn_ctx *rpn_ctx;
    const struct rpn_expression *filter;

    enum SelvaMergeStrategy merge_strategy;
    struct RedisModuleString *merge_path;
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
    /**
     * Fields that should be excluded when `fields` contains a wildcard.
     * The list should delimit the excluded fields in the following way:
     * ```
     * \0field1\0field2\0
     * ```
     */
    struct RedisModuleString *excluded_fields;
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

    const struct RedisModuleString *order_field; /*!< Order by field name; Otherwise NULL. */
    SVector *order_result; /*!< Results of the find wrapped in TraversalOrderedItem structs.
                            *   Only used if sorting is requested. */

    struct Selva_SubscriptionMarker *marker; /*!< Used by FindInSub. */

    /* Accounting */
    size_t acc_take; /*!< Numer of nodes selected during the traversal. */
    size_t acc_tot; /*!< Total number of nodes visited during the traversal. */
};

int SelvaTraversal_ParseOrder(
        const struct RedisModuleString **order_by_field,
        enum SelvaResultOrder *order,
        const struct RedisModuleString *txt,
        const struct RedisModuleString *fld,
        const struct RedisModuleString *ord);
int SelvaTraversal_ParseDir2(enum SelvaTraversal *dir, const struct RedisModuleString *arg);
orderFunc SelvaTraversal_GetOrderFunc(enum SelvaResultOrder order);
struct TraversalOrderedItem *SelvaTraversal_CreateOrderItem(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *lang,
        struct SelvaHierarchyNode *node,
        const struct RedisModuleString *order_field);
struct TraversalOrderedItem *SelvaTraversal_CreateObjectBasedOrderItem(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString *lang,
        struct SelvaObject *obj,
        const struct RedisModuleString *order_field);
int SelvaTraversal_FieldsContains(struct SelvaObject *fields, const char *field_name_str, size_t field_name_len);
int SelvaTraversal_GetSkip(enum SelvaTraversal dir);
const char *SelvaTraversal_Dir2str(enum SelvaTraversal dir);

#endif /* SELVA_TRAVERSAL */
