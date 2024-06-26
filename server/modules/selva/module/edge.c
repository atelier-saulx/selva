#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include "redismodule.h"
#include "auto_free.h"
#include "errors.h"
#include "svector.h"
#include "edge.h"
#include "hierarchy.h"
#include "selva_object.h"
#include "subscriptions.h"
#include "comparator.h"

static void clear_all_fields(RedisModuleCtx *ctx, struct SelvaModify_Hierarchy *hierarchy, struct SelvaModify_HierarchyNode *node);
static void EdgeField_Reply(struct RedisModuleCtx *ctx, void *p);
static void EdgeField_Free(void *p);
static size_t EdgeField_Len(void *p);
static void *EdgeField_RdbLoad(struct RedisModuleIO *io, int encver, void *data);
static void EdgeField_RdbSave(struct RedisModuleIO *io, void *value, void *data);

static const struct SelvaObjectPointerOpts obj_opts = {
    .ptr_type_id = SELVA_OBJECT_POINTER_EDGE,
    .ptr_reply = EdgeField_Reply,
    .ptr_free = EdgeField_Free,
    .ptr_len = EdgeField_Len,
    .ptr_save = EdgeField_RdbSave,
    .ptr_load = EdgeField_RdbLoad,
};
SELVA_OBJECT_POINTER_OPTS(obj_opts);

static void init_node_metadata_edge(
        Selva_NodeId id __unused,
        struct SelvaModify_HierarchyMetadata *metadata) {
    metadata->edge_fields.edges = NULL;
    metadata->edge_fields.origins = NULL;
}
SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(init_node_metadata_edge);

static void deinit_node_metadata_edge(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        struct SelvaModify_HierarchyNode *node,
        struct SelvaModify_HierarchyMetadata *metadata) {
    struct SelvaObject *origins;
    struct SelvaObject *edges;

    /*
     * Remove the edges pointing to this node.
     */
    origins = metadata->edge_fields.origins;
    if (origins) {
        void *obj_it;
        SVector *edge_fields;
        const char *src_node_id;

        obj_it = SelvaObject_ForeachBegin(origins);
        while ((edge_fields = SelvaObject_ForeachValue(origins, &obj_it, &src_node_id, SELVA_OBJECT_ARRAY))) {
            struct SVectorIterator vec_it;
            struct EdgeField *src_field;

            /*
             * Delete each edge connecting to this node.
             */
            SVector_ForeachBegin(&vec_it, edge_fields);
            while ((src_field = SVector_Foreach(&vec_it))) {
                SVector_Remove(&src_field->arcs, (void *)node);
            }
        }

        SelvaObject_Destroy(metadata->edge_fields.origins);
        metadata->edge_fields.origins = NULL;
    }

    /*
     * Remove the edges pointing from this node to other nodes.
     */
    edges = metadata->edge_fields.edges;
    if (edges) {
        clear_all_fields(ctx, hierarchy, node);
        SelvaObject_Destroy(edges);
        metadata->edge_fields.edges = NULL;
    }
}
SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(deinit_node_metadata_edge);

/**
 * Allocate a new EdgeField struct and initialize it.
 */
static struct EdgeField *alloc_EdgeField(const Selva_NodeId src_node_id, const struct EdgeFieldConstraint *constraint, size_t initial_size) {
    struct EdgeField *edgeField;

    edgeField = RedisModule_Calloc(1, sizeof(struct EdgeField));
    if (!edgeField) {
        return NULL;
    }

    edgeField->constraint = constraint;
    memcpy(edgeField->src_node_id, src_node_id, SELVA_NODE_ID_SIZE);
    SVector_Init(&edgeField->arcs, initial_size, SelvaSVectorComparator_Node);

    return edgeField;
}

struct EdgeField *Edge_GetField(const struct SelvaModify_HierarchyNode *src_node, const char *field_name_str, size_t field_name_len) {
    const struct SelvaModify_HierarchyMetadata *src_metadata;
    struct EdgeField *src_edge_field;
    int err;

    /* Some callers expect that src_node can be NULL. */
    if (!src_node) {
        return NULL;
    }

    /*
     * The edges object is allocated lazily so the called might need to allocate it.
     */
    src_metadata = SelvaHierarchy_GetNodeMetadataByPtr(src_node);
    if (!src_metadata->edge_fields.edges) {
        return NULL;
    }

    void *p;
    err = SelvaObject_GetPointerStr(src_metadata->edge_fields.edges, field_name_str, field_name_len, &p);
    src_edge_field = p;
    if (err) {
        return NULL;
    }

    return src_edge_field;
}

/**
 * Create a new edge field and store it on the hierarchy node.
 */
static struct EdgeField *Edge_NewField(
        struct SelvaModify_HierarchyNode *node,
        const char *field_name_str,
        size_t field_name_len,
        const struct EdgeFieldConstraint *constraint) {
    Selva_NodeId node_id;
    struct SelvaModify_HierarchyMetadata *node_metadata;
    struct SelvaObject *edges;
    struct EdgeField *edge_field;

    SelvaHierarchy_GetNodeId(node_id, node);
    node_metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);

    edges = node_metadata->edge_fields.edges;
    if (!edges) {
        edges = SelvaObject_New();
        if (!edges) {
            return NULL;
        }

        node_metadata->edge_fields.edges = edges;
    }

    edge_field = alloc_EdgeField(node_id, constraint, 0);
    if (!edge_field) {
        /* Just leave the edges obj there as it's already properly initialized. */
        return NULL;
    }

    SelvaObject_SetPointerStr(edges, field_name_str, field_name_len, edge_field, &obj_opts);

    return edge_field;
}

/**
 * Insert an edge.
 * The edge must not exist before calling this function because this
 * function doesn't perform any checks.
 */
static void insert_edge(struct EdgeField *src_edge_field, struct SelvaModify_HierarchyNode *dst_node) {
    struct SelvaModify_HierarchyMetadata *dst_node_metadata;
    int err;

    /*
     *  Insert the hierarchy node to the edge field.
     */
    SVector_InsertFast(&src_edge_field->arcs, dst_node);

    /*
     * Add origin reference.
     * Note that we must ensure that this insertion is only ever done once.
     */
    dst_node_metadata = SelvaHierarchy_GetNodeMetadataByPtr(dst_node);

    /* TODO can we avoid crashing in the following error cases? */
    if (!dst_node_metadata->edge_fields.origins) {
        /* The edge origin refs struct is initialized lazily. */
        dst_node_metadata->edge_fields.origins = SelvaObject_New();
        if (!dst_node_metadata->edge_fields.origins) {
            fprintf(stderr, "%s:%d: OOM while inserting an edge\n",
                    __FILE__, __LINE__);
            abort();
        }
    }
    err = SelvaObject_AddArrayStr(dst_node_metadata->edge_fields.origins,
                                  src_edge_field->src_node_id, SELVA_NODE_ID_SIZE,
                                  SELVA_OBJECT_POINTER, src_edge_field);
    if (err) {
        fprintf(stderr, "%s:%d: Edge origin update failed: %s\n",
                __FILE__, __LINE__,
                getSelvaErrorStr(err));
        abort();
    }
}

static int get_or_create_EdgeField(
        const struct EdgeFieldConstraints *constraints,
        struct SelvaModify_HierarchyNode *node,
        const char *field_name_str,
        size_t field_name_len,
        unsigned constraint_id,
        struct EdgeField **out) {
    Selva_NodeType node_type;
    const struct EdgeFieldConstraint *constraint;
    struct EdgeField *edge_field;

    SelvaHierarchy_GetNodeType(node_type, node);
    constraint = Edge_GetConstraint(constraints, constraint_id, node_type, field_name_str, field_name_len);
    if (!constraint) {
        return SELVA_EINVAL;
    }

    edge_field = Edge_GetField(node, field_name_str, field_name_len);
    if (!edge_field) {
        edge_field = Edge_NewField(node, field_name_str, field_name_len, constraint);
        if (!edge_field) {
            return SELVA_ENOMEM;
        }
    } else {
        if (edge_field->constraint != constraint) {
            return SELVA_EINVAL;
        }
        if (SVector_Search(&edge_field->arcs, node)) {
            return SELVA_EEXIST;
        }
    }

    *out = edge_field;
    return 0;
}

int Edge_Has(const struct EdgeField *edge_field, struct SelvaModify_HierarchyNode *dst_node) {
    return SVector_SearchIndex(&edge_field->arcs, dst_node) >= 0;
}

/* RFE Optimize by taking edgeField as an arg. */
int Edge_Add(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        unsigned constraint_id,
        const char *field_name_str,
        size_t field_name_len,
        struct SelvaModify_HierarchyNode *src_node,
        struct SelvaModify_HierarchyNode *dst_node) {
    const struct EdgeFieldConstraints *constraints = &hierarchy->edge_field_constraints;
    const struct EdgeFieldConstraint *constraint;
    struct EdgeField *src_edge_field = NULL;
    int err;

    assert(hierarchy);

    /*
     * Get src_edge_field
     */
    err = get_or_create_EdgeField(constraints, src_node, field_name_str, field_name_len, constraint_id, &src_edge_field);
    if (err) {
        return err;
    }

    if (Edge_Has(src_edge_field, dst_node)) {
        return SELVA_EEXIST;
    }

    constraint = src_edge_field->constraint;

    if (constraint->flags & EDGE_FIELD_CONSTRAINT_FLAG_SINGLE_REF) {
        int res;
        /*
         * single_ref allows only one edge to exist in the field.
         */
        res = Edge_ClearField(ctx, hierarchy, src_node, field_name_str, field_name_len);
        if (res < 0) {
            return res;
        }
    }

    insert_edge(src_edge_field, dst_node);

    if (constraint->flags & EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL) {
        /*
         * This field is bidirectional and so we need to create an edge pointing back.
         */
        err = Edge_Add(ctx, hierarchy, EDGE_FIELD_CONSTRAINT_DYNAMIC,
                       constraint->bck_field_name_str, constraint->bck_field_name_len,
                       dst_node, src_node);
        if (err && err != SELVA_EEXIST) {
            /*
             * Ok so, this is a bit dumb but we break an infinite loop by
             * ignoring SELVA_EEXIST. It's terribly inefficient to attempt to
             * create the same edge again just to figure that both ends were
             * created already. This implementation also practically allows
             * multidirectional edges between two nodes.
             * The normal flow should be like this:
             * Edge_Add(src, dst) -> Edge_Add(dst, src) -> Edge_Add(src, dst) => SELVA_EEXIST
             */
            /* TODO Actually handle errors here. */
            fprintf(stderr, "%s:%d: An error occurred while creating a bidirectional edge: %s\n",
                    __FILE__, __LINE__, getSelvaErrorStr(err));
        }
    }

    SelvaSubscriptions_InheritEdge(ctx, hierarchy, src_node, dst_node, field_name_str, field_name_len);
    /*
     * Note that the regular change events for edges are only sent by the modify
     * command. Therefore we expect we don't need to do anything here.
     */

    return 0;
}

static int remove_origin_ref(struct EdgeField *src_edge_field, struct SelvaModify_HierarchyNode *dst_node) {
    struct SelvaModify_HierarchyMetadata *metadata;
    enum SelvaObjectType out_subtype;
    SVector *origin_fields;
    int err;

    metadata = SelvaHierarchy_GetNodeMetadataByPtr(dst_node);
    err = SelvaObject_GetArrayStr(metadata->edge_fields.origins, src_edge_field->src_node_id, SELVA_NODE_ID_SIZE, &out_subtype, &origin_fields);
    if (err) {
        return err;
    }
    if (out_subtype != SELVA_OBJECT_POINTER) {
        return SELVA_EINTYPE;
    }

    /*
     * Delete the edge origin reference from the destination.
     */
    if (SVector_Size(origin_fields) == 1) {
        /*
         * We assume that the only remaining origin field reference is
         * edge_field, as it should be so if all the ref counting is done
         * correctly.
         */
        SelvaObject_DelKeyStr(metadata->edge_fields.origins, src_edge_field->src_node_id, SELVA_NODE_ID_SIZE);
    } else {
        /* Otherwise remove the specific edge_field from the SVector. */
        ssize_t i;

        i = SVector_SearchIndex(origin_fields, src_edge_field);
        if (i != -1) {
            SVector_RemoveIndex(origin_fields, i);
        }
    }

    return 0;
}

/**
 * Delete markers that are related to edge traversing markers reaching from src to dst.
 * This function allows the client to create any markers on the dst node and
 * get them cleaned up automatically when the edge between the two nodes is
 * removed. A related marker is one that has the same subscription id as the
 * marker on the destination node and it starts from the source node.
 */
static void remove_related_edge_markers(
        struct RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        struct SelvaModify_HierarchyNode *src_node,
        struct SelvaModify_HierarchyNode *dst_node) {
    SVECTOR_AUTOFREE(sub_markers);
    struct SVectorIterator dst_it;
    struct Selva_SubscriptionMarker *dst_marker;
    const struct SelvaModify_HierarchyMetadata *src_meta;
    const struct SelvaModify_HierarchyMetadata *dst_meta;
    Selva_NodeId src_node_id;

    src_meta = SelvaHierarchy_GetNodeMetadataByPtr(src_node);
    dst_meta = SelvaHierarchy_GetNodeMetadataByPtr(dst_node);
    SelvaHierarchy_GetNodeId(src_node_id, src_node);

    if (unlikely(!SVector_Clone(&sub_markers, &dst_meta->sub_markers.vec, NULL))) {
        fprintf(stderr, "%s:%d: ENOMEM failed to remove sub markers from a referenced node\n",
                __FILE__, __LINE__);
        return;
    }

    SVector_ForeachBegin(&dst_it, &sub_markers);
    while ((dst_marker = SVector_Foreach(&dst_it))) {
        struct SVectorIterator src_it;
        const struct Selva_SubscriptionMarker *src_marker;

        if ((dst_marker->dir &
             (SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
              SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD |
              SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
              SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) &&
            !memcmp(dst_marker->node_id, src_node_id, SELVA_NODE_ID_SIZE)) {
            /*
             * Skip markers that are fixable with a clear & refresh,
             * edge traversing markers, that are actually important for
             * determining which markers are related to the edge subscription.
             */
            continue;
        }

        /*
         * The contract is that we must delete all markers that belong to the
         * same subscription as an edge traversing marker on the source node.
         */
        SVector_ForeachBegin(&src_it, &src_meta->sub_markers.vec);
        while ((src_marker = SVector_Foreach(&src_it))) {
            if ((src_marker->dir &
                 (SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD |
                  SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD |
                  SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION |
                  SELVA_HIERARCHY_TRAVERSAL_EXPRESSION)) &&
                src_marker->sub == dst_marker->sub &&
                !memcmp(src_marker->node_id, src_node_id, SELVA_NODE_ID_SIZE)) {
                /* RFE Is it a bit ugly to do this here? */
                /* TODO Should it be dst_node or src_node here? */
                dst_marker->marker_action(ctx, hierarchy, dst_marker, SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY, dst_node);

                (void)SelvaSubscriptions_DeleteMarkerByPtr(ctx, hierarchy, dst_marker);
            }
        }
    }
}

int Edge_Delete(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        struct EdgeField *edge_field,
        struct SelvaModify_HierarchyNode *src_node,
        Selva_NodeId dst_node_id) {
    Selva_NodeId src_node_id;
    struct EdgeField *src_edge_field = edge_field;
    struct SelvaModify_HierarchyNode *dst_node;
    int err;

    SelvaHierarchy_GetNodeId(src_node_id, src_node);
    dst_node = SVector_Search(&src_edge_field->arcs, dst_node_id);
    if (!dst_node) {
        return SELVA_ENOENT;
    }

    /*
     * ctx is not set if the function is called from RDB functions or some
     * initialization functions in hierarchy.
     */
    if (ctx) {
        remove_related_edge_markers(ctx, hierarchy, src_node, dst_node);
        /* TODO We should probably clear from the dst? */
        /* TODO We don't probably need to clear all markers, just those that are using the same traversal. */
        SelvaSubscriptions_ClearAllMarkers(ctx, hierarchy, src_node);
    }

    err = remove_origin_ref(src_edge_field, dst_node);
    if (err) {
        return err;
    }

    /*
     * Delete the edge.
     */
    SVector_Remove(&src_edge_field->arcs, dst_node_id);

    /*
     * For bidirectional edge fields we need to also remove the edge directed
     * back from the destination node.
     */
    const struct EdgeFieldConstraint *src_constraint = edge_field->constraint;
    if (src_constraint && src_constraint->flags & EDGE_FIELD_CONSTRAINT_FLAG_BIDIRECTIONAL) {
        struct EdgeField *bck_edge_field;
        const char *bck_field_name_str = src_constraint->bck_field_name_str;
        size_t bck_field_name_len = src_constraint->bck_field_name_len;

        bck_edge_field = Edge_GetField(dst_node, bck_field_name_str, bck_field_name_len);
        err = Edge_Delete(ctx, hierarchy, bck_edge_field, dst_node, src_node_id);
        if (err && err != SELVA_ENOENT) {
            fprintf(stderr, "%s:%d: Failed to to remove a backwards edge of a bidirectional edge field\n",
                    __FILE__, __LINE__);
        }
    } else if (!src_constraint) {
        fprintf(stderr, "%s:%d: Source field constraint not set\n",
                __FILE__, __LINE__);
    }

    return 0;
}

static int clear_field(RedisModuleCtx *ctx, struct SelvaModify_Hierarchy *hierarchy, struct SelvaModify_HierarchyNode *src_node, struct EdgeField *edge_field) {
    SVECTOR_AUTOFREE(arcs);
    struct SVectorIterator it;
    const struct SelvaModify_HierarchyNode *dst_node;

    if (unlikely(!SVector_Clone(&arcs, &edge_field->arcs, NULL))) {
        return SELVA_ENOMEM;
    }

    SVector_ForeachBegin(&it, &arcs);
    while ((dst_node = SVector_Foreach(&it))) {
        Selva_NodeId dst_node_id;
        int err;

        SelvaHierarchy_GetNodeId(dst_node_id, dst_node);
        err = Edge_Delete(
                ctx,
                hierarchy,
                edge_field,
                src_node,
                dst_node_id);
        if (err) {
            fprintf(stderr, "%s:%d:%s: Unable to delete an edge: %s\n",
                    __FILE__, __LINE__, __func__,
                    getSelvaErrorStr(err));
        }
    }

    return (int)SVector_Size(&arcs);
}

static void _clear_all_fields(RedisModuleCtx *ctx, struct SelvaModify_Hierarchy *hierarchy, struct SelvaModify_HierarchyNode *node, struct SelvaObject *obj) {
    SelvaObject_Iterator *it;
    enum SelvaObjectType type;
    void *p;

    it = SelvaObject_ForeachBegin(obj);
    while ((p = SelvaObject_ForeachValueType(obj, &it, NULL, &type))) {
        if (type == SELVA_OBJECT_POINTER) {
            clear_field(ctx, hierarchy, node, p);
        } else if (type == SELVA_OBJECT_OBJECT) {
            _clear_all_fields(ctx, hierarchy, node, p);
        } else {
            fprintf(stderr, "%s:%d: edges should not contain %s values\n",
                    __FILE__, __LINE__,
                    SelvaObject_Type2String(type, NULL));
        }
    }
}

static void clear_all_fields(RedisModuleCtx *ctx, struct SelvaModify_Hierarchy *hierarchy, struct SelvaModify_HierarchyNode *node) {
    struct SelvaObject *obj = SelvaHierarchy_GetNodeMetadataByPtr(node)->edge_fields.edges;

    _clear_all_fields(ctx, hierarchy, node, obj);
}

int Edge_ClearField(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        struct SelvaModify_HierarchyNode *src_node,
        const char *field_name_str,
        size_t field_name_len) {
    Selva_NodeType node_type;
    struct EdgeField *src_edge_field;

    if (!src_node) {
        return SELVA_ENOENT;
    }

    SelvaHierarchy_GetNodeType(node_type, src_node);

    src_edge_field = Edge_GetField(src_node, field_name_str, field_name_len);
    if (!src_edge_field) {
        return SELVA_ENOENT;
    }

    return clear_field(ctx, hierarchy, src_node, src_edge_field);
}

int Edge_DeleteField(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        struct SelvaModify_HierarchyNode *src_node,
        const char *field_name_str,
        size_t field_name_len) {
    Selva_NodeType src_node_type;
    const struct EdgeField *src_edge_field;
    int res;

    SelvaHierarchy_GetNodeType(src_node_type, src_node);

    src_edge_field = Edge_GetField(src_node, field_name_str, field_name_len);
    if (!src_edge_field) {
        return SELVA_ENOENT;
    }

    res = Edge_ClearField(ctx, hierarchy, src_node, field_name_str, field_name_len);
    if (res < 0) {
        return res;
    }

    /*
     * Doing this will cause a full cleanup of the edges and origin pointers (EdgeField_Free()).
     */
    if (SelvaObject_DelKeyStr(SelvaHierarchy_GetNodeMetadataByPtr(src_node)->edge_fields.edges, field_name_str, field_name_len)) {
        fprintf(stderr, "%s:%d: Failed to delete the edge field: \"%.*s\"",
                __FILE__, __LINE__,
                (int)field_name_len, field_name_str);
    }

    return 0;
}

int Edge_Refcount(struct SelvaModify_HierarchyNode *node) {
    struct SelvaModify_HierarchyMetadata *metadata;
    int refcount = 0;

    metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);

    if (metadata->edge_fields.origins) {
        refcount = (int)SelvaObject_Len(metadata->edge_fields.origins, NULL);
    }

    return refcount;
}

static void EdgeField_Reply(struct RedisModuleCtx *ctx, void *p) {
    const struct EdgeField *edge_field = (struct EdgeField *)p;
    const SVector *arcs = &edge_field->arcs;
    const struct SelvaModify_HierarchyNode *dst_node;
    struct SVectorIterator it;

    RedisModule_ReplyWithArray(ctx, SVector_Size(arcs));

    SVector_ForeachBegin(&it, arcs);
    while ((dst_node = SVector_Foreach(&it))) {
        Selva_NodeId dst_node_id;

        SelvaHierarchy_GetNodeId(dst_node_id, dst_node);
        RedisModule_ReplyWithStringBuffer(ctx, dst_node_id, Selva_NodeIdLen(dst_node_id));
    }
}

void replyWithEdgeField(struct RedisModuleCtx *ctx, struct EdgeField *edge_field) {
    EdgeField_Reply(ctx, edge_field);
}

static void EdgeField_Free(void *p) {
    struct EdgeField *edge_field = (struct EdgeField *)p;

    /*
     * We don't need to call clear_field() here if we manage to always call it
     * before this function is called, and thus we avoid passing the context.
     */
#if 0
    clear_all_fields(ctx, hierarchy, node, edge_field);
#endif
    SVector_Destroy(&edge_field->arcs);
    RedisModule_Free(p);
}

static size_t EdgeField_Len(void *p) {
    const struct EdgeField *edge_field = (struct EdgeField *)p;

    return SVector_Size(&edge_field->arcs);
}

struct EdgeField_load_data {
    struct SelvaModify_Hierarchy *hierarchy;
    struct SelvaModify_HierarchyNode *src_node;
};

/*
 * A custom SelvaObject pointer RDB loader for EdgeFields.
 * Storage format: [
 *   constraint_id,
 *   nr_edges,
 *   dst_id
 * ]
 */
static void *EdgeField_RdbLoad(struct RedisModuleIO *io, __unused int encver, void *p) {
    RedisModuleCtx *ctx = RedisModule_GetContextFromIO(io);
    struct EdgeField_load_data *load_data = (struct EdgeField_load_data *)p;
    struct SelvaModify_Hierarchy *hierarchy = load_data->hierarchy;
    Selva_NodeId src_node_id;
    unsigned constraint_id;
    const struct EdgeFieldConstraint *constraint;
    size_t nr_edges;
    struct EdgeField *edge_field;

    constraint_id = RedisModule_LoadUnsigned(io);

    if (constraint_id == EDGE_FIELD_CONSTRAINT_DYNAMIC) {
        char *node_type __auto_free = NULL;
        char *field_name_str __auto_free = NULL;
        size_t field_name_len;

        node_type = RedisModule_LoadStringBuffer(io, NULL);
        field_name_str = RedisModule_LoadStringBuffer(io, &field_name_len);
        constraint = Edge_GetConstraint(&hierarchy->edge_field_constraints, constraint_id, node_type, field_name_str, field_name_len);
    } else {
        constraint = Edge_GetConstraint(&hierarchy->edge_field_constraints, constraint_id, "NA", "", 0);
    }

    nr_edges = RedisModule_LoadUnsigned(io);

    SelvaHierarchy_GetNodeId(src_node_id, load_data->src_node);
    edge_field = alloc_EdgeField(src_node_id, constraint, nr_edges);
    if (!edge_field) {
        return NULL;
    }

    for (size_t i = 0; i < nr_edges; i++) {
        char *dst_id_str __auto_free = NULL;
        size_t dst_id_len;
        struct SelvaModify_HierarchyNode *dst_node;
        int err;

        dst_id_str = RedisModule_LoadStringBuffer(io, &dst_id_len);
        if (dst_id_len != SELVA_NODE_ID_SIZE) {
            return NULL;
        }

        /*
         * Ensure that the destination node exist before creating an edge.
         */
        err = SelvaModify_AddHierarchy(ctx, hierarchy, dst_id_str, 0, NULL, 0, NULL);
        if (err < 0) {
            RedisModule_LogIOError(io, "warning", "AddHierarchy(%.*s) failed: %s",
                                   (int)SELVA_NODE_ID_SIZE, dst_id_str,
                                   getSelvaErrorStr(err));
            return NULL;
        }

        dst_node = SelvaHierarchy_FindNode(hierarchy, dst_id_str);
        if (!unlikely(dst_node)) {
            return NULL;
        }

        insert_edge(edge_field, dst_node);
    }

    return edge_field;
}

int Edge_RdbLoad(struct RedisModuleIO *io, int encver, SelvaModify_Hierarchy *hierarchy, struct SelvaModify_HierarchyNode *node) {
    RedisModuleCtx *ctx = RedisModule_GetContextFromIO(io);

    if (encver < 1) { /* hierarchy encver */
        return 0; /* Only the latest version supports loading metadata. */
    }

    if (unlikely(!ctx)) {
        RedisModule_LogIOError(io, "warning", "Redis ctx can't be NULL");
        return SELVA_EINVAL;
    }

    /* A boolean flag to tell whether there are any edge fields. */
    if (RedisModule_LoadUnsigned(io)) {
        struct SelvaModify_HierarchyMetadata *metadata;

        metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);

        /*
         * We use the SelvaObject RDB loader to load the object which will then
         * call EdgeField_RdbLoad for each field stored in the object to
         * initialize the actual EdgeField structures.
         */
        metadata->edge_fields.edges = SelvaObjectTypeRDBLoad(io, encver, &(struct EdgeField_load_data){
            .hierarchy = hierarchy,
            .src_node = node,
        });
    }

    return 0;
}

/**
 * Custom RDB save function for saving EdgeFields.
 */
static void EdgeField_RdbSave(struct RedisModuleIO *io, void *value, __unused void *save_data) {
    const struct EdgeField *edgeField = (struct EdgeField *)value;
    unsigned constraint_id = edgeField->constraint ? edgeField->constraint->constraint_id : EDGE_FIELD_CONSTRAINT_ID_DEFAULT;
    struct SVectorIterator vec_it;
    const struct SelvaModify_HierarchyNode *dst_node;

    RedisModule_SaveUnsigned(io, constraint_id);
    if (constraint_id == EDGE_FIELD_CONSTRAINT_DYNAMIC) {
        const struct EdgeFieldConstraint *constraint = edgeField->constraint;

        RedisModule_SaveStringBuffer(io, constraint->node_type, SELVA_NODE_TYPE_SIZE);
        RedisModule_SaveStringBuffer(io, constraint->field_name_str, constraint->field_name_len);
    }
    RedisModule_SaveUnsigned(io, SVector_Size(&edgeField->arcs)); /* nr_edges */

    SVector_ForeachBegin(&vec_it, &edgeField->arcs);
    while ((dst_node = SVector_Foreach(&vec_it))) {
        Selva_NodeId dst_node_id;

        SelvaHierarchy_GetNodeId(dst_node_id, dst_node);
        RedisModule_SaveStringBuffer(io, dst_node_id, SELVA_NODE_ID_SIZE);
    }
}

void Edge_RdbSave(struct RedisModuleIO *io, struct SelvaModify_HierarchyNode *node) {
    struct SelvaModify_HierarchyMetadata *metadata = SelvaHierarchy_GetNodeMetadataByPtr(node);
    struct SelvaObject *edges = metadata->edge_fields.edges;

    /* A boolean marker to trigger the loader. */
    if (edges && SelvaObject_Len(edges, NULL) > 0) {
        RedisModule_SaveUnsigned(io, 1);
        SelvaObjectTypeRDBSave(io, metadata->edge_fields.edges, NULL);
    } else {
        RedisModule_SaveUnsigned(io, 0);
    }
}
