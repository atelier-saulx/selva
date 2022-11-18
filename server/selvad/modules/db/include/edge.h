/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _EDGE_H_
#define _EDGE_H_

/*
 * Custom edge field management.
 */

#include "util/svector.h"
#include "selva_db.h"
#include "selva_object.h"

struct RedisModuleCtx;
struct RedisModuleIO;
struct RedisModuleString;
struct SelvaHierarchy;
struct SelvaHierarchyNode;
struct SelvaObject;

/*
 * Constraint ids.
 */
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
enum EdgeFieldConstraintFlag {
    EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF       = 0x01, /*!< Single reference edge. */
    EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL    = 0x02, /*!< Bidirectional reference. */
    EDGE_FIELD_CONSTRAINT_FLAG_DYNAMIC          = 0x04, /*!< Lookup from dynamic constraints by node type and field_name. */
} __packed;

struct EdgeFieldDynConstraintParams {
    enum EdgeFieldConstraintFlag flags;
    Selva_NodeType src_node_type;
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
    enum EdgeFieldConstraintFlag flags;

    Selva_NodeType src_node_type; /*!< Source node type this constraint applies to. */

    /**
     * Forward traversing field of this constraint.
     */
    char *field_name_str;
    size_t field_name_len;

    /**
     * Constraint of the backwards traversing field.
     * Used if the EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL flag is set.
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
    STATIC_SELVA_OBJECT(dyn_constraints);
};

/**
 * A struct for edge fields.
 * This struct contains the actual arcs pointing directly to other nodes in the
 * hierarchy.
 */
struct EdgeField {
    const struct EdgeFieldConstraint *constraint; /*!< A pointer to the constraint of this edge field. */
    struct SVector arcs; /*!< Pointers to hierarchy nodes. */
    Selva_NodeId src_node_id; /*!< Source/owner nodeId of this edge field. */
    struct SelvaObject *metadata; /*!< Metadata organized by dst_node_id. Can be NULL. */
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

void Edge_InitEdgeFieldConstraints(struct EdgeFieldConstraints *efc);
void Edge_DeinitEdgeFieldConstraints(struct EdgeFieldConstraints *efc);
int Edge_NewDynConstraint(struct EdgeFieldConstraints *efc, const struct EdgeFieldDynConstraintParams *params);
const struct EdgeFieldConstraint *Edge_GetConstraint(
        const struct EdgeFieldConstraints *efc,
        unsigned constraint_id,
        Selva_NodeType node_type,
        const char *field_name_str,
        size_t field_name_len);

/**
 * Assess the usage of Edge features in a hierarchy node.
 * @returns 0 if no Edge features are used in the given node;
 *          1 if this node has edge fields;
 *          2 if edge fields in other nodes are pointing to this node;
 *          3 both 1 and 2.
 */
int Edge_Usage(const struct SelvaHierarchyNode *node);

/**
 * Get a pointer to an EdgeField.
 * Note that the pointer returned is guaranteed to be valid only during the
 * execution of the current command.
 * @param node is a pointer to the node the lookup should be applied to. Can be NULL.
 * @returns A pointer to an EdgeField if node is set and the field is found; Otherwise NULL.
 */
struct EdgeField *Edge_GetField(const struct SelvaHierarchyNode *node, const char *field_name_str, size_t field_name_len);

static inline size_t Edge_GetFieldLength(const struct EdgeField *edge_field) {
    return SVector_Size(&edge_field->arcs);
}

static inline enum EdgeFieldConstraintFlag Edge_GetFieldConstraintFlags(const struct EdgeField *edge_field) {
    return edge_field->constraint->flags;
}

/**
 * Get a pointer to the metadata object of an EdgeField.
 * @param edge_field is a pointer to the EdgeField.
 * @param create if set the object will be created if it didn't exist before.
 * @returns A pointer to the metadata object; Otherwise a NULL pointer is returned.
 */
struct SelvaObject *Edge_GetFieldMetadata(struct EdgeField *edge_field, int create);

/**
 * Get a pointer to the metadata of an edge in the EdgeField.
 */
int Edge_GetFieldEdgeMetadata(struct EdgeField *edge_field, const Selva_NodeId dst_node_id, int create, struct SelvaObject **out);

/**
 * Delete all metadata from edge_field.
 */
void Edge_DeleteFieldMetadata(struct EdgeField *edge_field);

/**
 * Check if an EdgeField has a reference to dst_node.
 * @returns 0 = not found;
 *          1 = found.
 */
int Edge_Has(const struct EdgeField *edge_field, struct SelvaHierarchyNode *dst_node);
int Edge_HasNodeId(const struct EdgeField *edge_field, const Selva_NodeId dst_node_id);

/**
 * Deref the node from a single ref edge field.
 * @param edge_field is a pointer to the edge field.
 * @param[out] node_out is se to the the pointer to a node edge_field is pointing to. Can be NULL.
 * @returns 0 if succeed and node_out was set; SELVA_EINTYPE if edge_field is not a single ref field; SELVA_ENOENT if edge_field was empty.
 */
int Edge_DerefSingleRef(const struct EdgeField *edge_field, struct SelvaHierarchyNode **node_out);

static inline void Edge_ForeachBegin(struct SVectorIterator *it, const struct EdgeField *edge_field) {
    SVector_ForeachBegin(it, &edge_field->arcs);
}

static inline struct SelvaHierarchyNode *Edge_Foreach(struct SVectorIterator *it) {
    return SVector_Foreach(it);
}

/**
 * Add a new edge.
 * If the field doesn't exist it will be created using the given constraint_id.
 * If the field exists but the constraint_id doesn't match to the currently set
 * constraint then the function will return SELVA_EINVAL.
 */
int Edge_Add(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        unsigned constraint_id,
        const char *field_name_str,
        size_t field_name_len,
        struct SelvaHierarchyNode *src_node,
        struct SelvaHierarchyNode *dst_node) __attribute__((nonnull (1, 2)));
int Edge_Delete(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct EdgeField *edge_field,
        struct SelvaHierarchyNode *src_node,
        Selva_NodeId dst_node_id);

/**
 * Delete all edges of a field.
 * @returns The number of deleted edges; Otherwise a selva error is returned.
 */
int Edge_ClearField(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *src_node,
        const char *field_name_str,
        size_t field_name_len);
int Edge_DeleteField(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *src_node,
        const char *field_name_str,
        size_t field_name_len);

/**
 * Get the number of nodes pointing to this nodes from edge fields.
 * Note that this isn't the number of edges as one node may have
 * multiple edges from separate edge fields pointing to the same destination
 * node.
 * @param node is a pointer to the node.
 * @returns Returns the number of references from other nodes.
 */
size_t Edge_Refcount(struct SelvaHierarchyNode *node);

void replyWithEdgeField(struct RedisModuleCtx *ctx, struct EdgeField *edge_field);

int Edge_RdbLoad(struct RedisModuleIO *io, int encver, struct SelvaHierarchy *hierarchy, struct SelvaHierarchyNode *node);
void Edge_RdbSave(struct RedisModuleIO *io, struct SelvaHierarchyNode *node);
int EdgeConstraint_RdbLoad(struct RedisModuleIO *io, int encver, struct EdgeFieldConstraints *data);
void EdgeConstraint_RdbSave(struct RedisModuleIO *io, struct EdgeFieldConstraints *data);

#endif /* _EDGE_H_ */
