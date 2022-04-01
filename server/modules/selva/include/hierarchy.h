#pragma once
#ifndef _SELVA_HIERARCHY_H_
#define _SELVA_HIERARCHY_H_

#include "redismodule.h"
#include "linker_set.h"
#include "selva.h"
#include "svector.h"
#include "tree.h"
#include "trx.h"
#include "edge.h"
#include "poptop.h"
#include "selva_object.h"
#include "selva_set.h"
#include "subscriptions.h"

#define HIERARCHY_ENCODING_VERSION  4

/* Forward declarations */
struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaHierarchy;
struct SelvaHierarchyNode;
struct Selva_Subscription;
struct ida;
/* End of forward declarations */

typedef struct SelvaHierarchy SelvaHierarchy;

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
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        struct SelvaHierarchyMetadata *metadata);

/**
 * Hierarchy node metadata constructor.
 * Declare a hook function that should be called when a new node is being
 * created. The function signature is SelvaHierarchyMetadataConstructorHook.
 */
#define SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(fun) \
    DATA_SET(selva_HMCtor, fun)

/**
 * Hierarchy node metadata destructor.
 * Declare a hook function that should be called when a node is being
 * destroyed. The function signature is SelvaHierarchyMetadataDestructorHook.
 */
#define SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(fun) \
    DATA_SET(selva_HMDtor, fun)

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
        int proc_timer_active; /*!< The indexing decission proc timer is active. */
        RedisModuleTimerID proc_timer_id; /*!< The indexing decission proc timer id. */
        struct ida *ida; /*!< Id allocator for subscription marker ids. */
        struct poptop top_indices; /*!< A list of top requested indices. */
        struct SelvaObject *index_map;
    } dyn_index;

    /**
     * State for inactive nodes tracking.
     * These are nodes potentially moving to the detached hierarchy.
     */
    struct {
        /**
         * Inactive nodeIds.
         * Inactive node ids are listed here on RDB save for further
         * processing. This is a pointer to a memory region shared with the
         * RDB child process.
         * NodeIds listed here have been inactive for a long time and are
         * potential candidates for compression.
         */
        Selva_NodeId *nodes;
        size_t nr_nodes;
        size_t next; /*!< Next empty slot in inactive_nodes. */

        /**
         * A timer used by auto compression.
         */
        RedisModuleTimerID auto_compress_timer;
    } inactive;

    /**
     * Storage descriptor for detached nodes.
     * It's possible to determine if a node exists in a detached subtree and restore
     * the node and its subtree using this structure.
     */
    struct {
        /**
         * The object maps each detached nodeId to a pointer that describes where
         * the detached subtree containing the nodeId is located. E.g. it can be
         * a tagged pointer to a RedisModuleString that contains a compressed
         * subtree string.
         */
        struct SelvaObject *obj;
    } detached;
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

/**
 * Callback descriptor used for traversals.
 */
struct SelvaHierarchyCallback {
    /**
     * Called for each orphan head in the hierarchy.
     */
    SelvaHierarchyHeadCallback head_cb;
    void * head_arg;

    /**
     * Called for each node in the hierarchy.
     */
    SelvaHierarchyNodeCallback node_cb;
    void * node_arg;

    /**
     * Called for each child of current node.
     */
    SelvaHierarchyChildCallback child_cb;
    void * child_arg;

    enum SelvaHierarchyCallbackFlags {
        SELVA_HIERARCHY_CALLBACK_FLAGS_INHIBIT_RESTORE = 0x01,
    } flags;
};

/**
 * Flags for SelvaModify_DelHierarchyNode().
 */
enum SelvaModify_DelHierarchyNodeFlag {
    DEL_HIERARCHY_NODE_FORCE = 0x01, /*!< Force delete regardless of existing parents and external edge references. */
    DEL_HIERARCHY_NODE_DETACH = 0x02, /*!< Delete, mark as detached. Note that this doesn't disable sending subscription events. */
    DEL_HIERARCHY_NODE_REPLY_IDS = 0x04, /*!< Send the deleted nodeIds as a reply to the client. */
};

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

/**
 * Copy nodeId to a buffer.
 * @param[out] id is a pointer to a Selva_NodeId.
 * @param node is a pointer to a hierarchy node.
 * @returns id.
 */
static inline char *SelvaHierarchy_GetNodeId(Selva_NodeId id, const struct SelvaHierarchyNode *node) {
    const char *buf = (const char *)node;

    /* We know the id is the first thing in the struct. */
    memcpy(id, buf, SELVA_NODE_ID_SIZE);

    return id;
}

/**
 * Get the type of a node.
 * @param[out] type is a pointer to char array that can hold a node type.
 * @param node is a pointer to a hierarchy node.
 * @returns type.
 */
static inline char *SelvaHierarchy_GetNodeType(char type[SELVA_NODE_TYPE_SIZE], const struct SelvaHierarchyNode *node) {
    const char *buf = (const char *)node;

    memcpy(type, buf, SELVA_NODE_TYPE_SIZE);

    return type;
}

/**
 * Get the SelvaObject of a hierarchy node.
 * @returns a pointer to the SelvaObject of node.
 */
struct SelvaObject *SelvaHierarchy_GetNodeObject(const struct SelvaHierarchyNode *node);

const struct SelvaHierarchyMetadata *_SelvaHierarchy_GetNodeMetadataByConstPtr(const struct SelvaHierarchyNode *node);
struct SelvaHierarchyMetadata *_SelvaHierarchy_GetNodeMetadataByPtr(struct SelvaHierarchyNode *node);
/**
 * Get node metadata by a pointer to the node.
 */
#define SelvaHierarchy_GetNodeMetadataByPtr(node) _Generic((node), \
        const struct SelvaHierarchyNode *: _SelvaHierarchy_GetNodeMetadataByConstPtr, \
        struct SelvaHierarchyNode *: _SelvaHierarchy_GetNodeMetadataByPtr \
        )(node)

/**
 * Get node metadata by nodeId.
 */
struct SelvaHierarchyMetadata *SelvaHierarchy_GetNodeMetadata(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id);

#if HIERARCHY_SORT_BY_DEPTH
ssize_t SelvaModify_GetHierarchyDepth(SelvaHierarchy *hierarchy, const Selva_NodeId id);
#endif

/**
 * Clear all user fields of a node SelvaObject.
 */
int SelvaHierarchy_ClearNodeFields(struct SelvaObject *obj);

/**
 * Delete all child edges of a node.
 */
void SelvaHierarchy_DelChildren(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node);
/**
 * Delete all parent edges of a node.
 */
void SelvaHierarchy_DelParents(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node);

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
        const Selva_NodeId *children,
        struct SelvaHierarchyNode **node_out);

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
 * @returns The total number of nodes deleted; Otherwise an error code is returned.
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
 * Check if node exists.
 */
static inline int SelvaHierarchy_NodeExists(SelvaHierarchy *hierarchy, const Selva_NodeId id) {
    return SelvaHierarchy_FindNode(hierarchy, id) != NULL;
}

/**
 * Get orphan head nodes of the given hierarchy.
 */
ssize_t SelvaModify_GetHierarchyHeads(SelvaHierarchy *hierarchy, Selva_NodeId **res);

void SelvaHierarchy_TraverseChildren(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb);
void SelvaHierarchy_TraverseParents(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseBFSAncestors(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseBFSDescendants(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_Traverse(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaTraversal dir,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseField(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaTraversal dir,
        const char *field_name_str,
        size_t field_name_len,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseExpression(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        struct rpn_ctx *rpn_ctx,
        const struct rpn_expression *rpn_expr,
        struct rpn_ctx *edge_filter_ctx,
        const struct rpn_expression *edge_filter,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseExpressionBfs(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        struct rpn_ctx *rpn_ctx,
        const struct rpn_expression *rpn_expr,
        struct rpn_ctx *edge_filter_ctx,
        const struct rpn_expression *edge_filter,
        const struct SelvaHierarchyCallback *cb);
/**
 * Foreach value in an array field.
 */
int SelvaHierarchy_TraverseArray(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        const char *field_str,
        size_t field_len,
        const struct SelvaObjectArrayForeachCallback *cb);
/**
 * Foreach value in a set field.
 */
int SelvaHierarchy_TraverseSet(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        const char *field_str,
        size_t field_len,
        const struct SelvaObjectSetForeachCallback *cb);
/**
 * Foreach value in a set-like field.
 * Traverse each value (foreach) in a field.
 * Supported fields:
 * - parents
 * - children
 * - ancestors
 * - descendants
 * - string and numeric array fields
 * - string and numeric set fields
 */
int SelvaHierarchy_ForeachInField(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len,
        const struct SelvaObjectSetForeachCallback *cb);

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
