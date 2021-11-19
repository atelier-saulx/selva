#pragma once
#ifndef _SELVA_HIERARCHY_H_
#define _SELVA_HIERARCHY_H_

#include "linker_set.h"
#include "selva.h"
#include "svector.h"
#include "tree.h"
#include "trx.h"
#include "edge.h"
#include "subscriptions.h"
#include "poptop.h"

#define HIERARCHY_ENCODING_VERSION  3

struct SelvaHierarchy;
typedef struct SelvaHierarchy SelvaHierarchy;
struct SelvaHierarchyNode;

/* Forward declarations for metadata */
/* ... */
/* End of forward declarations for metadata */

/**
 * Hierarchy node metadata.
 * This structure should contain primitive data types or pointers to forward
 * declared structures.
 */
struct SelvaHierarchyMetadata {
    /**
     * Subscription markers.
     */
    struct Selva_SubscriptionMarkers sub_markers;
    struct EdgeFieldContainer edge_fields;
};

typedef void SelvaHierarchyMetadataConstructorHook(
        const Selva_NodeId id,
        struct SelvaHierarchyMetadata *metadata);
typedef void SelvaHierarchyMetadataDestructorHook(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaHierarchyMetadata *metadata);

#define SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(fun) \
    DATA_SET(selva_HMCtor, fun)

#define SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(fun) \
    DATA_SET(selva_HMDtor, fun)

struct Selva_Subscription;

RB_HEAD(hierarchy_index_tree, SelvaHierarchyNode);
RB_HEAD(hierarchy_subscriptions_tree, Selva_Subscription);

struct SelvaHierarchy {
    /**
     * Global transaction state.
     */
    struct trx_state trx_state;

    /**
     * Index of all hierarchy nodes by ID.
     */
    struct hierarchy_index_tree index_head;

    /**
     * Orphan nodes aka heads of the hierarchy.
     */
    SVector heads;

    /**
     * Edge field constraints.
     */
    struct EdgeFieldConstraints edge_field_constraints;

    struct {
        /**
         * A tree of all subscriptions applying to this tree.
         */
        struct hierarchy_subscriptions_tree head;

        /**
         * Subscription markers for missing accessors (nodeIds and aliases).
         *
         * These are single-shot markers that will be deleted once the
         * condition is met. The markers are stored only in this object in
         * the following format:
         *
         * ```
         * {
         *   nodeIdOrAlias.subId => struct Selva_Subscription *
         * }
         * ```
         *
         * When a subscription is removed the markers for missing nodes should
         * be deleted.
         */
        struct SelvaObject *missing;

        /**
         * Special subscription markers.
         * Possible reasons to add a subscription marker to this list are:
         * - the marker is applying to all nodes starting from the root node
         *   towards descendants
         * - the marker is applying to all nodes
         * - the marker is applying to new nodes
         * - the marker is applying to deletions
         */
        struct Selva_SubscriptionMarkers detached_markers;

        /**
         * Deferred subscription events.
         * The events are deduplicated by subscription ID and the events will
         * be sent out when SelvaSubscriptions_SendDeferredEvents() is called.
         *
         * The intended type in this list is struct Selva_Subscription.
         */
        struct SelvaSubscriptions_DeferredEvents deferred_events;
    } subs;

    struct {
        int nr_indices; /*!< Total number of active indices. */
        struct bitmap *find_marker_id_stack;
        struct poptop top_indices; /*!< A list of top requested indices. */

        struct indexing_timer_args *indexing_timer_args;
        struct SelvaObject *index_map;
    } dyn_index;

    /**
     * Storage descriptor for detached nodes.
     * It's possible to determine if a node exists in a detached subtree and restore
     * the node and its subtree using this structure.
     */
    struct SelvaHierarchyDetached {
        /**
         * The object maps each nodeId to a pointer that describes where the detached
         * subtree containing the nodeId is located. E.g. it can be a tagged pointer
         * to a RedisModuleString that contains a compressed subtree string.
         */
        struct SelvaObject *obj;
    } index_detached;
};

/**
 * Called for each node found during a traversal.
 * @returns 0 to continue the traversal; 1 to interrupt the traversal.
 */
typedef int (*SelvaHierarchyCallback)(struct SelvaHierarchyNode *node, void *arg);

struct SelvaHierarchyCallback {
    SelvaHierarchyCallback node_cb;
    void * node_arg;
};

typedef int (*SelvaModify_ArrayObjectCallback)(struct SelvaObject *obj, void *arg);

struct SelvaModify_ArrayObjectCallback {
    SelvaModify_ArrayObjectCallback node_cb;
    void * node_arg;
};

/**
 * Flags for SelvaModify_DelHierarchyNode().
 */
enum SelvaModify_DelHierarchyNodeFlag {
    DEL_HIERARCHY_NODE_FORCE = 0x01, /*!< Force delete regardless of existing parents and external edge references. */
    DEL_HIERARCHY_NODE_DETACH = 0x02, /*!< Delete, mark as detached. Note that this doesn't disable sending subscription events. */
};

struct RedisModuleCtx;
struct RedisModuleString;

/**
 * Create a new hierarchy.
 */
SelvaHierarchy *SelvaModify_NewHierarchy(struct RedisModuleCtx *ctx);

/**
 * Free a hierarchy.
 */
void SelvaModify_DestroyHierarchy(SelvaHierarchy *hierarchy);

/**
 * Open a hierarchy key.
 */
SelvaHierarchy *SelvaModify_OpenHierarchy(struct RedisModuleCtx *ctx, struct RedisModuleString *key_name, int mode);

int SelvaHierarchy_NodeExists(SelvaHierarchy *hierarchy, const Selva_NodeId id);

/**
 * Copy nodeId to a buffer.
 */
static inline char *SelvaHierarchy_GetNodeId(Selva_NodeId id, const struct SelvaHierarchyNode *node) {
    const char *buf = (const char *)node;

    /* We know the id is the first thing in the struct. */
    memcpy(id, buf, SELVA_NODE_ID_SIZE);

    return id;
}

static inline char *SelvaHierarchy_GetNodeType(char type[SELVA_NODE_TYPE_SIZE], const struct SelvaHierarchyNode *node) {
    const char *buf = (const char *)node;

    memcpy(type, buf, SELVA_NODE_TYPE_SIZE);

    return type;
}

struct SelvaObject *SelvaHierarchy_GetNodeObject(const struct SelvaHierarchyNode *node);

const struct SelvaHierarchyMetadata *_SelvaHierarchy_GetNodeMetadataByConstPtr(const struct SelvaHierarchyNode *node);
struct SelvaHierarchyMetadata *_SelvaHierarchy_GetNodeMetadataByPtr(struct SelvaHierarchyNode *node);
#define SelvaHierarchy_GetNodeMetadataByPtr(node) _Generic((node), \
        const struct SelvaHierarchyNode *: _SelvaHierarchy_GetNodeMetadataByConstPtr, \
        struct SelvaHierarchyNode *: _SelvaHierarchy_GetNodeMetadataByPtr \
        )(node)

struct SelvaHierarchyMetadata *SelvaHierarchy_GetNodeMetadata(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id);

#if HIERARCHY_SORT_BY_DEPTH
ssize_t SelvaModify_GetHierarchyDepth(SelvaHierarchy *hierarchy, const Selva_NodeId id);
#endif

int SelvaModify_DelHierarchyChildren(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id);

int SelvaModify_DelHierarchyParents(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id);

/**
 * Set node relationships relative to other existing nodes.
 * Previously existing connections to and from other nodes are be removed.
 * If a node with id doesn't exist it will be created.
 * @param parents   Sets these nodes and only these nodes as parents of this node.
 * @param children  Sets these nodes and only these nodes as children of this node.
 */
int SelvaModify_SetHierarchy(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children);

/**
 * Set parents of an existing node.
 * @param parents   Sets these nodes and only these nodes as parents of this node.
 */
int SelvaModify_SetHierarchyParents(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents);

/**
 * Set children of an existing node.
 * @param children  Sets these nodes and only these nodes as children of this node.
 */
int SelvaModify_SetHierarchyChildren(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_children,
        const Selva_NodeId *children);

int SelvaHierarchy_UpsertNode(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        struct SelvaHierarchyNode **out);

/**
 * Add new relationships relative to other existing nodes.
 * The function is nondestructive; previously existing edges to and from other
 * nodes and metadata are be preserved.
 * @param ctx If NULL then no events are sent.
 * @param parents   Sets these nodes as parents to this node,
 *                  while keeping the existing parents.
 * @param children  Sets these nodes as children to this node,
 *                  while keeping the existing children.
 */
int SelvaModify_AddHierarchyP(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children);

/**
 * Add new relationships relative to other existing nodes.
 * The function is nondestructive; previously existing edges to and from other
 * nodes and metadata are be preserved.
 * If a node with id doesn't exist it will be created.
 * @param ctx If NULL then no events are sent.
 * @param parents   Sets these nodes as parents to this node,
 *                  while keeping the existing parents.
 * @param children  Sets these nodes as children to this node,
 *                  while keeping the existing children.
 */
int SelvaModify_AddHierarchy(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children);

/**
 * Remove relationship relative to other existing nodes.
 * @param parents   Removes the child relationship between this node and
 *                  the listed parents.
 * @param children  Removes the parent relationship between this node and
 *                  the listed children.
 */
int SelvaModify_DelHierarchy(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children);

/**
 * Delete a node from the hierarchy.
 * @param force if non-zero the even children that have other relationships will
 *              be deleted.
 */
int SelvaModify_DelHierarchyNode(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaModify_DelHierarchyNodeFlag flags);

/**
 * Get an opaque pointer to a hierarchy node.
 * Do not use this function unless you absolutely need it as the safest and
 * better supporter way to refer to hierarchy nodes is by using nodeId.
 */
struct SelvaHierarchyNode *SelvaHierarchy_FindNode(SelvaHierarchy *hierarchy, const Selva_NodeId id);

/**
 * Get orphan head nodes of the given hierarchy.
 */
ssize_t SelvaModify_GetHierarchyHeads(SelvaHierarchy *hierarchy, Selva_NodeId **res);

int SelvaModify_TraverseHierarchy(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaTraversal dir,
        const struct SelvaHierarchyCallback *cb);
int SelvaModify_TraverseHierarchyField(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaTraversal dir,
        const char *field_name_str,
        size_t field_name_len,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseExpression(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        struct rpn_ctx *rpn_ctx,
        struct rpn_expression *rpn_expr,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseExpressionBfs(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        struct rpn_ctx *rpn_ctx,
        const struct rpn_expression *rpn_expr,
        const struct SelvaHierarchyCallback *cb);
int SelvaModify_TraverseArray(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        const char *ref_field_str,
        size_t ref_field_len,
        const struct SelvaModify_ArrayObjectCallback *cb);
int SelvaHierarchy_IsNonEmptyField(const struct SelvaHierarchyNode *node, const char *field_str, size_t field_len);

/*
 * hierarchy_reply.c
 */

/**
 * Reply with a hierarchy traversal.
 * [nodeId1, nodeId2,.. nodeIdn]
 */
int HierarchyReply_WithTraversal(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId nodeId,
        size_t nr_types,
        const Selva_NodeType *types,
        enum SelvaTraversal dir);

#endif /* _SELVA_HIERARCHY_H_ */
