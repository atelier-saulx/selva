#pragma once
#ifndef _EDGE_H_
#define _EDGE_H_

/*
 * Custom edge field management.
 */

#include "selva.h"
#include "svector.h"

struct RedisModuleCtx;
struct RedisModuleIO;
struct RedisModuleString;
struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyNode;
struct SelvaObject;

#define EDGE_FIELD_CONSTRAINT_ID_DEFAULT    0
#define EDGE_FIELD_CONSTRAINT_SINGLE_REF    1
#define EDGE_FIELD_CONSTRAINT_DYNAMIC       2

/*
 * EdgeFieldConstraint Flags
 * -------------------------
 *
 * Bidirectional references:
 * If one edge is removed the other edge is removed too. This flag requires
 * that fwd_field, and bck_field are set.
 */
#define EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF       0x01 /*!< Single reference edge. */
#define EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL    0x02 /*!< Bidirectional reference. */
#define EDGE_FIELD_CONSTRAINT_FLAG_DYNAMIC          0x04 /*!< Lookup from dynamic constraints by node type and field_name. */

struct EdgeFieldDynConstraintParams {
    unsigned flags;
    Selva_NodeType fwd_node_type;
    struct RedisModuleString *fwd_field_name;
    struct RedisModuleString *bck_field_name;
};

/**
 * Edge constraint.
 * Edge constraints controls how an edge field behaves on different operations
 * like arc insertion and deletion or hierarchy node deletion.
 */
struct EdgeFieldConstraint {
    unsigned constraint_id;

    /**
     * Constraint flags controlling the behaviour.
     */
    unsigned flags;

    Selva_NodeType node_type;

    /**
     * Forward traversing field of this constraint.
     */
    char *field_name_str;
    size_t field_name_len;

    /**
     * Constraint of the backwards traversing field.
     * Used if the EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL flag is set.
     * TODO Figure out how to do this.
     */
    char *bck_field_name_str;
    size_t bck_field_name_len;
};

/**
 * Edge field constraints per hierarchy key.
 * Edge field constraints are insert only and the only way to clear all the
 * constraints is by deleting the whole structure, meaning deleting the
 * hierarchy key.
 * Each constraint has an unique constraint_id given at creation time which also
 * corresponds its location in the constraints array of this structure.
 * This structure should be accessed with the following functions:
 *
 * - Edge_InitEdgeFieldConstraints(),
 * - Edge_NewConstraint(),
 * - Edge_GetConstraint(),
 */
struct EdgeFieldConstraints {
    struct EdgeFieldConstraint hard_constraints[2];
    struct SelvaObject *dyn_constraints;
};

/**
 * A struct for edge fields.
 * This struct contains the actual arcs pointing directly to other nodes in the
 * hierarchy.
 */
struct EdgeField {
    const struct EdgeFieldConstraint *constraint; /*!< A pointer to the constraint of this edge field. */
    Selva_NodeId src_node_id; /*!< Source/owner nodeId of this edge field. */
    struct SVector arcs; /*!< Pointers to hierarchy nodes. */
};

/*
 * Hierarchy node metadata structure for storing edges and references to the
 * origin EdgeFields.
 */
struct EdgeFieldContainer {
    /**
     * Custom edge fields.
     *
     * A.field -> B
     * {
     *   custom.field: <struct EdgeField>
     * }
     */
    struct SelvaObject *edges;
    /**
     * Custom edge field origin references.
     * This object contains pointers to each field pointing to this node. As
     * it's organized per nodeId the size of the object tells how many nodes
     * are pointing to this node via edge fields.
     *
     * A.field <- B
     * {
     *   nodeId1: [     // The node pointing to this node
     *     fieldPtr1,   // A pointer to the edgeField pointing to this node
     *     fieldPtr2,
     *   ],
     * }
     */
    struct SelvaObject *origins;
};

void Edge_InitEdgeFieldConstraints(struct EdgeFieldConstraints *data);
int Edge_NewDynConstraint(struct EdgeFieldConstraints *data, struct EdgeFieldDynConstraintParams *params);
const struct EdgeFieldConstraint *Edge_GetConstraint(const struct EdgeFieldConstraints *data, unsigned constraint_id, Selva_NodeType node_type, const char *field_name_str, size_t field_name_len);

/**
 * Get a pointer to an EdgeField.
 * Note that the pointer returned is guaranteed to be valid only during the
 * execution of the current command.
 * @param node is a pointer to the node the lookup should be applied to. Can be NULL.
 * @returns A pointer to an EdgeField if node is set and the field is found; Otherwise NULL.
 */
struct EdgeField *Edge_GetField(struct SelvaModify_HierarchyNode *node, const char *field_name_str, size_t field_name_len);

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
int Edge_Add(
        struct RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        unsigned constraint_id,
        const char *field_name_str,
        size_t field_name_len,
        struct SelvaModify_HierarchyNode *src_node,
        struct SelvaModify_HierarchyNode *dst_node);
int Edge_Delete(
        struct RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        struct EdgeField *edge_field,
        struct SelvaModify_HierarchyNode *src_node,
        Selva_NodeId dst_node_id);

/**
 * Delete all edges of a field.
 * @returns The number of deleted edges; Otherwise a selva error is returned.
 */
int Edge_ClearField(struct RedisModuleCtx *ctx, struct SelvaModify_Hierarchy *hierarchy, struct SelvaModify_HierarchyNode *src_node, const char *field_name_str, size_t field_name_len);
int Edge_DeleteField(struct RedisModuleCtx *ctx, struct SelvaModify_Hierarchy *hierarchy, struct SelvaModify_HierarchyNode *src_node, const char *field_name_str, size_t field_name_len);

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
int EdgeConstraint_RdbLoad(struct RedisModuleIO *io, int encver, struct EdgeFieldConstraints *data);
void EdgeConstraint_RdbSave(struct RedisModuleIO *io, struct EdgeFieldConstraints *data);

#endif /* _EDGE_H_ */
