#pragma once
#ifndef SELVA_MODIFY_HIERARCHY
#define SELVA_MODIFY_HIERARCHY

#define SELVA_NODE_ID_SIZE      10ul
#define SELVA_NODE_TYPE_SIZE    2
#define ROOT_NODE_ID            "root\0\0\0\0\0\0"

/*
 * Error codes.
 */

/**
 * General error.
 */
#define SELVA_MODIFY_HIERARCHY_EGENERAL (-1)
/**
 * Operation not supported.
 */
#define SELVA_MODIFY_HIERARCHY_ENOTSUP  (-2)
/**
 * Out of memory.
 */
#define SELVA_MODIFY_HIERARCHY_ENOMEM   (-3)
/**
 * Node or entity not found.
 */
#define SELVA_MODIFY_HIERARCHY_ENOENT   (-4)
/**
 * Node or entity already exist.
 */
#define SELVA_MODIFY_HIERARCHY_EEXIST   (-5)


#define HIERARCHY_DEFAULT_KEY "___selva_hierarchy"

typedef char Selva_NodeId[SELVA_NODE_ID_SIZE];
struct SelvaModify_Hierarchy;
typedef struct SelvaModify_Hierarchy SelvaModify_Hierarchy;

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
SelvaModify_Hierarchy *SelvaModify_OpenHierarchyKey(struct RedisModuleCtx *ctx, struct RedisModuleString *key_name);

int SelvaModify_HierarchyNodeExists(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id);

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

/**
 * Add new relationships relative to other existing nodes.
 * Previously existing connections to and from other nodes are be preserved.
 * If a node with id doesn't exist it will be created.
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

extern const char * const hierarchyStrError[6];

#endif /* SELVA_MODIFY_HIERARCHY */
