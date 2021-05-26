#pragma once
#ifndef SELVA_MODIFY_HIERARCHY
#define SELVA_MODIFY_HIERARCHY

#include "linker_set.h"
#include "selva.h"
#include "svector.h"
#include "tree.h"
#include "trx.h"
#include "edge.h"
#include "subscriptions.h"

#define HIERARCHY_ENCODING_VERSION  1

struct SelvaModify_Hierarchy;
typedef struct SelvaModify_Hierarchy SelvaModify_Hierarchy;
struct SelvaModify_HierarchyNode;

/* Forward declarations for metadata */
/* ... */
/* End of forward declarations for metadata */

/**
 * Hierarchy node metadata.
 * This structure should contain primitive data types or pointers to forward
 * declared structures.
 */
struct SelvaModify_HierarchyMetadata {
    /**
     * Subscription markers.
     */
    struct Selva_SubscriptionMarkers sub_markers;
    struct EdgeFieldContainer edge_fields;
};

typedef void SelvaModify_HierarchyMetadataHook(const Selva_NodeId id, struct SelvaModify_HierarchyMetadata *metadata);

#define SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(fun) \
    DATA_SET(selva_HMCtor, fun)

#define SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(fun) \
    DATA_SET(selva_HMDtor, fun)

struct Selva_Subscription;

RB_HEAD(hierarchy_index_tree, SelvaModify_HierarchyNode);
RB_HEAD(hierarchy_subscriptions_tree, Selva_Subscription);

struct SelvaModify_Hierarchy {
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

    struct {
        /**
         * A tree of all subscriptions applying to this tree.
         */
        struct hierarchy_subscriptions_tree head;

        /**
         * Subscription markers for missing accessors (nodeIds and aliases).
         *
         * These are single-shot markers that will be deleted once the
         * condition is met.
         *
         * These are stored only in this object in the following format:
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
};

/**
 * Called for each node found during a traversal.
 * @returns 0 to continue the traversal; 1 to interrupt the traversal.
 */
typedef int (*SelvaModify_HierarchyCallback)(struct SelvaModify_HierarchyNode *node, void *arg);

struct SelvaModify_HierarchyCallback {
    SelvaModify_HierarchyCallback node_cb;
    void * node_arg;
};

typedef int (*SelvaModify_ArrayObjectCallback)(struct SelvaObject *obj, void *arg);

struct SelvaModify_ArrayObjectCallback {
    SelvaModify_ArrayObjectCallback node_cb;
    void * node_arg;
};

struct RedisModuleCtx;
struct RedisModuleString;

/**
 * Create a new hierarchy.
 */
SelvaModify_Hierarchy *SelvaModify_NewHierarchy(struct RedisModuleCtx *ctx);

/**
 * Free a hierarchy.
 */
void SelvaModify_DestroyHierarchy(SelvaModify_Hierarchy *hierarchy);

/**
 * Open a hierarchy key.
 */
SelvaModify_Hierarchy *SelvaModify_OpenHierarchy(struct RedisModuleCtx *ctx, struct RedisModuleString *key_name, int mode);

int SelvaModify_HierarchyNodeExists(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id);

/**
 * Copy nodeId to a buffer.
 */
char *SelvaModify_HierarchyGetNodeId(Selva_NodeId id, const struct SelvaModify_HierarchyNode *node);

struct SelvaModify_HierarchyMetadata *SelvaModify_HierarchyGetNodeMetadataByPtr(struct SelvaModify_HierarchyNode *node);
struct SelvaModify_HierarchyMetadata *SelvaModify_HierarchyGetNodeMetadata(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id);

#if HIERARCHY_SORT_BY_DEPTH
ssize_t SelvaModify_GetHierarchyDepth(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id);
#endif

int SelvaModify_DelHierarchyChildren(
        struct RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id);

int SelvaModify_DelHierarchyParents(
        struct RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
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
        SelvaModify_Hierarchy *hierarchy,
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
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents);

/**
 * Set children of an existing node.
 * @param children  Sets these nodes and only these nodes as children of this node.
 */
int SelvaModify_SetHierarchyChildren(
        struct RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_children,
        const Selva_NodeId *children);

int SelvaHierarchy_UpsertNode(
        struct RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        struct SelvaModify_HierarchyNode **out);

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
        SelvaModify_Hierarchy *hierarchy,
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
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children);

/**
 * Delete a node from the hierarchy.
 */
int SelvaModify_DelHierarchyNode(
        struct RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id);

/**
 * Get an opaque pointer to a hierarchy node.
 * Do not use this function unless you absolutely need it as the safest and
 * better supporter way to refer to hierarchy nodes is by using nodeId.
 */
struct SelvaModify_HierarchyNode *SelvaHierarchy_FindNode(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id);

/**
 * Get orphan head nodes of the given hierarchy.
 */
ssize_t SelvaModify_GetHierarchyHeads(SelvaModify_Hierarchy *hierarchy, Selva_NodeId **res);

/**
 * Get an unsorted list of ancestors fo a given node.
 */
ssize_t SelvaModify_FindAncestors(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **ancestors);

/**
 * Get an unsorted list of descendants of a given node.
 */
ssize_t SelvaModify_FindDescendants(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **descendants);

const char *SelvaModify_HierarchyDir2str(enum SelvaModify_HierarchyTraversal dir);
int SelvaModify_TraverseHierarchy(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaModify_HierarchyTraversal dir,
        const struct SelvaModify_HierarchyCallback *cb);
int SelvaModify_TraverseHierarchyRef(
        struct RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        const char *ref_field,
        const struct SelvaModify_HierarchyCallback *cb);
int SelvaModify_TraverseHierarchyEdge(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        const char *field_name_str,
        size_t field_name_len,
        const struct SelvaModify_HierarchyCallback *cb);
int SelvaHierarchy_TraverseExpression(
        struct RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        struct rpn_ctx *rpn_ctx,
        struct rpn_expression *rpn_expr,
        const struct SelvaModify_HierarchyCallback *cb);
int SelvaModify_TraverseArray(
        struct RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        const char *ref_field,
        const struct SelvaModify_ArrayObjectCallback *cb);
int SelvaHierarchy_IsNonEmptyField(struct SelvaModify_HierarchyNode *node, const char *field_str, size_t field_len);

/*
 * hierarchy_reply.c
 */

/**
 * Reply with a hierarchy traversal.
 * [nodeId1, nodeId2,.. nodeIdn]
 */
int HierarchyReply_WithTraversal(
        struct RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId nodeId,
        size_t nr_types,
        const Selva_NodeType *types,
        enum SelvaModify_HierarchyTraversal dir);

#endif /* SELVA_MODIFY_HIERARCHY */
