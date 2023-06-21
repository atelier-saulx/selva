/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _SELVA_HIERARCHY_H_
#define _SELVA_HIERARCHY_H_

#include "linker_set.h"
#include "selva_db.h"
#include "util/svector.h"
#include "util/mempool.h"
#include "util/trx.h"
#include "util/poptop.h"
#include "tree.h"
#include "edge.h"
#include "selva_object.h"
#include "selva_set.h"
#include "subscriptions.h"

#define HIERARCHY_ENCODING_VERSION  5

/* Forward declarations */
struct SelvaHierarchy;
struct SelvaHierarchyNode;
struct Selva_Subscription;
struct ida;
struct selva_server_response_out;
struct selva_string;
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

    int flag_isSaving;

    /**
     * Index of all hierarchy nodes by ID.
     */
    struct hierarchy_index_tree index_head;
    struct mempool node_pool;

    /**
     * Orphan nodes aka heads of the hierarchy.
     */
    SVector heads;

    /**
     * Node types.
     */
    struct {
        STATIC_SELVA_OBJECT(_obj_data);
    } types;

    /**
     * Aliases.
     */
    struct {
        STATIC_SELVA_OBJECT(_obj_data);
    } aliases;

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
        struct {
            STATIC_SELVA_OBJECT(_obj_data);
        } missing;

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
        int proc_timer_id; /*!< The indexing decission proc timer id. */
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
         * Inactive node ids are listed here during serialization for further
         * processing. This is a pointer to a memory region shared with the
         * serialization child process. We can access it lock free because we
         * know exactly when it's being read and thus can avoid writing it at
         * those times. NodeIds listed here have been inactive for a long time
         * and are potential candidates for compression.
         */
        Selva_NodeId *nodes;
        size_t nr_nodes;
        size_t next; /*!< Next empty slot in inactive_nodes. */

        /**
         * A timer used by auto compression.
         */
        int auto_compress_timer;
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
         * a tagged pointer to a selva_string that contains a compressed
         * subtree string.
         */
        struct SelvaObject *obj;
    } detached;
};

/**
 * Callback descriptor used for traversals.
 */
struct SelvaHierarchyCallback {
    /**
     * Called for each orphan head in the hierarchy.
     */
    SelvaHierarchyHeadCallback head_cb;
    void *head_arg;

    /**
     * Called for each node in the hierarchy.
     */
    SelvaHierarchyNodeCallback node_cb;
    void *node_arg;

    /**
     * Called for each child of current node.
     */
    SelvaHierarchyChildCallback child_cb;
    void *child_arg;

    enum SelvaHierarchyCallbackFlags {
        SELVA_HIERARCHY_CALLBACK_FLAGS_INHIBIT_RESTORE = 0x01,
    } flags;
};

#define SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy) \
    GET_STATIC_SELVA_OBJECT(&((hierarchy)->types))

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
SelvaHierarchy *SelvaModify_NewHierarchy(void);

/**
 * Free a hierarchy.
 */
void SelvaModify_DestroyHierarchy(SelvaHierarchy *hierarchy);

/**
 * Get the type name for a type prefix.
 * It's the caller's responsibility to call selva_string_free() for the
 * returned string.
 */
struct selva_string *SelvaHierarchyTypes_Get(struct SelvaHierarchy *hierarchy, const Selva_NodeType type);

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

int SelvaHierarchy_ClearNodeFlagImplicit(struct SelvaHierarchyNode *node);

/**
 * Clear all user fields of a node SelvaObject.
 */
void SelvaHierarchy_ClearNodeFields(struct SelvaObject *obj);

/**
 * Delete all child edges of a node.
 */
int SelvaHierarchy_DelChildren(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node);
/**
 * Delete all parent edges of a node.
 */
int SelvaHierarchy_DelParents(
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
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents);

/**
 * Set children of an existing node.
 * @param children  Sets these nodes and only these nodes as children of this node.
 */
int SelvaModify_SetHierarchyChildren(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_children,
        const Selva_NodeId *children);

int SelvaHierarchy_UpsertNode(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        struct SelvaHierarchyNode **out);

/**
 * Add new relationships relative to other existing nodes.
 * The function is nondestructive; previously existing edges to and from other
 * nodes and metadata are be preserved.
 * @param parents   Sets these nodes as parents to this node,
 *                  while keeping the existing parents.
 * @param children  Sets these nodes as children to this node,
 *                  while keeping the existing children.
 */
int SelvaModify_AddHierarchyP(
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
 * @param parents   Sets these nodes as parents to this node,
 *                  while keeping the existing parents.
 * @param children  Sets these nodes as children to this node,
 *                  while keeping the existing children.
 */
int SelvaModify_AddHierarchy(
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
        struct selva_server_response_out *resp,
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
 * Alias to node_id.
 */
int get_alias_str(struct SelvaHierarchy *hierarchy, const char *ref_str, size_t ref_len, Selva_NodeId node_id);

/**
 * Alias to node_id.
 */
int get_alias(struct SelvaHierarchy *hierarchy, const struct selva_string *ref, Selva_NodeId node_id);

/**
 * Remove an alias.
 * Caller must update the node aliases if necessary.
 */
int delete_alias(struct SelvaHierarchy *hierarchy, struct selva_string *ref);

/**
 * Remove aliases listed in set.
 * Caller must update the node aliases if necessary.
 */
int delete_aliases(struct SelvaHierarchy *hierarchy, struct SelvaSet *set);

/**
 * Update alias into the aliases key and remove the previous alias.
 * Caller must set the alias to the new node.
 */
void update_alias(SelvaHierarchy *hierarchy, const Selva_NodeId node_id, struct selva_string *ref);

/**
 * Get orphan head nodes of the given hierarchy.
 */
ssize_t SelvaModify_GetHierarchyHeads(SelvaHierarchy *hierarchy, Selva_NodeId **res);

void SelvaHierarchy_TraverseChildren(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb);
void SelvaHierarchy_TraverseParents(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseBFSAncestors(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseBFSDescendants(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_Traverse(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaTraversal dir,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseField(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaTraversal dir,
        const char *field_name_str,
        size_t field_name_len,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseExpression(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        struct rpn_ctx *rpn_ctx,
        const struct rpn_expression *rpn_expr,
        struct rpn_ctx *edge_filter_ctx,
        const struct rpn_expression *edge_filter,
        const struct SelvaHierarchyCallback *cb);
int SelvaHierarchy_TraverseExpressionBfs(
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
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        const char *field_str,
        size_t field_len,
        const struct SelvaObjectArrayForeachCallback *cb);
/**
 * Foreach value in a set field.
 */
int SelvaHierarchy_TraverseSet(
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
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId nodeId,
        size_t nr_types,
        const Selva_NodeType *types,
        enum SelvaTraversal dir);

SelvaHierarchy *Hierarchy_Load(struct selva_io *io);
void Hierarchy_Save(struct selva_io *io, SelvaHierarchy *hierarchy);

extern SelvaHierarchy *main_hierarchy;

#endif /* _SELVA_HIERARCHY_H_ */
