#pragma once
#ifndef SELVA_TRAVERSAL_H
#define SELVA_TRAVERSAL_H

#include "svector.h"
#include "arg_parser.h"

struct FindCommand_Args;
struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaHierarchy;
struct SelvaHierarchyNode;

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

extern const struct SelvaArgParser_EnumType merge_types[3];

struct SelvaNodeSendParam {
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

struct FindCommand_Args {
    struct RedisModuleString *lang;

    ssize_t *nr_nodes; /*!< Number of nodes in the result. */
    ssize_t offset; /*!< Start from nth node. */
    ssize_t *limit; /*!< Limit the number of result. */

    struct rpn_ctx *rpn_ctx;
    const struct rpn_expression *filter;

    struct SelvaNodeSendParam send_param;
    size_t *merge_nr_fields;

    const struct RedisModuleString *order_field; /*!< Order by field name; Otherwise NULL. */
    SVector *order_result; /*!< Results of the find wrapped in TraversalOrderItem structs.
                            *   Only used if sorting is requested. */

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
int SelvaTraversal_FieldsContains(struct SelvaObject *fields, const char *field_name_str, size_t field_name_len);
int SelvaTraversal_GetSkip(enum SelvaTraversal dir);
const char *SelvaTraversal_Dir2str(enum SelvaTraversal dir);

#endif /* SELVA_TRAVERSAL_H */
