#pragma once
#ifndef _EDGE_H_
#define _EDGE_H_

/*
 * Custom edge field management.
 */

#include "selva.h"
#include "svector.h"

struct RedisModuleIO;
struct RedisModuleCtx;
struct EdgeFieldConstraint;
struct EdgeField;
struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyNode;
struct SelvaObject;

struct EdgeFieldConstraint {
    struct {
        unsigned single_ref : 1;    /*!< Single reference edge. */
    } flags;
};

struct EdgeField {
    unsigned constraint_id; /*!< An index in the constraints array edge_constraints. */
    Selva_NodeId src_node_id; /*!< Source nodeId of this edge field. */
    struct SVector arcs; /*!< Pointers to nodes. */
};

/*
 * Hierarchy node metadata structure for storing references to the origin
 * EdgeFields.
 */
struct EdgeFieldContainer {
    /**
     * Custom edge fields.
     * A.field -> B
     *
     * {
     *   custom.field: <struct EdgeField>
     * }
     */
    struct SelvaObject *edges;
    /**
     * Custom edge field origin references.
     * A.field <- B
     *
     * {
     *   nodeId1: [     // The node pointing to this node
     *     fieldPtr1,   // A pointer to the edgeField pointing to this node
     *     fieldPtr2,
     *   ],
     * }
     */
    struct SelvaObject *origins;
};

const struct EdgeFieldConstraint *Edge_GetConstraint(unsigned constraint_id);
const struct EdgeFieldConstraint* Edge_GetFieldConstraint(const struct EdgeField *edge_field);

/**
 * Get a pointer to an EdgeField.
 * Note that the pointer returned is guaranteed to be valid only during the
 * execution of the current command.
 * @param node is a pointer to the node the lookup should be applied to. Can be NULL.
 * @returns A pointer to an EdgeField if node is set and the field is found; Otherwise NULL.
 */
struct EdgeField *Edge_GetField(struct SelvaModify_HierarchyNode *node, const char *key_name_str, size_t key_name_len);

/**
 * Check if an EdgeField has a reference to dst_node.
 * @returns 0 = not found;
 *          1 = found.
 */
int Edge_Has(struct EdgeField *edge_field, struct SelvaModify_HierarchyNode *dst_node);

/**
 * Add a new edge.
 * If the field doesn't exist it will be created using the given constraint_id.
 * If the field exists but the constraint_id doesn't match to the currently set
 * constraint then the function will return SELVA_EINVAL.
 */
int Edge_Add(const char *key_name_str, size_t key_name_len, unsigned constraint_id, struct SelvaModify_HierarchyNode *src_node, struct SelvaModify_HierarchyNode *dst_node);
int Edge_Delete(struct EdgeField *edge_field, struct SelvaModify_HierarchyNode *src_node, Selva_NodeId dst_node_id);

/**
 * Delete all edges of a field.
 * @returns The number of deleted edges; Otherwise a selva error is returned.
 */
int Edge_ClearField(struct SelvaModify_HierarchyNode *src_node, const char *key_name_str, size_t key_name_len);
int Edge_DeleteField(struct SelvaModify_HierarchyNode *src_node, const char *key_name_str, size_t key_name_len);

/**
 * Get the number of nodes pointing to this nodes from edge fields.
 * Note that this isn't the number of edges as one node may have
 * multiple edges from separate edge fields pointing to the same destination
 * node.
 * @param node is a pointer to the node.
 * @returns Returns the number of references from other nodes.
 */
int Edge_Refcount(struct SelvaModify_HierarchyNode *node);
void replyWithEdgeField(struct RedisModuleCtx *ctx, struct EdgeField *edge_field);

int Edge_RdbLoad(struct RedisModuleIO *io, int encver, struct SelvaModify_Hierarchy *hierarchy, struct SelvaModify_HierarchyNode *node);
void Edge_RdbSave(struct RedisModuleIO *io, struct SelvaModify_HierarchyNode *node);

#endif /* _EDGE_H_ */
