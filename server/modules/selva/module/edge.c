#include <stddef.h>
#include "redismodule.h"
#include "errors.h"
#include "edge.h"
#include "svector.h"
#include "selva_object.h"
#include "hierarchy.h"
#include "comparator.h"

struct EdgeFieldConstraint {
    struct {
        unsigned single_ref : 1;    /*!< Single reference edge. */
    } flags;
};

static void clear_field(struct EdgeField *src_edge_field);
static void EdgeField_Reply(struct RedisModuleCtx *ctx, void *p);
static void EdgeField_Free(void *p);
static size_t EdgeField_Len(void *p);
static void *EdgeField_RdbLoad(struct RedisModuleIO *io, int encver, void *data);
static void EdgeField_RdbSave(struct RedisModuleIO *io, void *value, void *data);

/**
 * Edge constraints.
 * All the constraints available in the database must be defined here.
 */
static const struct EdgeFieldConstraint edge_constraints[] = {
    [0] = {
        .flags = {
            .single_ref = 0,
        },
    },
    [1] = {
        .flags = {
            .single_ref = 1,
        },
    },
};

static const struct SelvaObjectPointerOpts obj_opts = {
    .ptr_type_id = SELVA_OBJECT_POINTER_EDGE,
    .ptr_reply = EdgeField_Reply,
    .ptr_free = EdgeField_Free,
    .ptr_len = EdgeField_Len,
    .ptr_save = EdgeField_RdbSave,
    .ptr_load = EdgeField_RdbLoad,
};

static void init_node_metadata_edge(
        Selva_NodeId id __unused,
        struct SelvaModify_HierarchyMetadata *metadata) {
    metadata->edge_fields.edges = NULL;
    metadata->edge_fields.origins = NULL;
}
SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(init_node_metadata_edge);

static void deinit_node_metadata_edge(const Selva_NodeId node_id, struct SelvaModify_HierarchyMetadata *metadata) {
    struct SelvaObject *origins;
    void *obj_it;
    SVector *edge_fields;
    const char *src_node_id;

    origins = metadata->edge_fields.origins;
    if (!origins) {
        /* No edges pointing to this node. */
        return;
    }

    obj_it = SelvaObject_ForeachBegin(origins);
    while ((edge_fields = (SVector *)SelvaObject_ForeachValue(origins, &obj_it, &src_node_id, SELVA_OBJECT_ARRAY))) {
        struct SVectorIterator vec_it;
        struct EdgeField *src_field;

        /*
         * Delete each edge connecting to this node.
         */
        SVector_ForeachBegin(&vec_it, edge_fields);
        while ((src_field = SVector_Foreach(&vec_it))) {
            SVector_Remove(&src_field->arcs, (void *)node_id);
        }
    }

    SelvaObject_Destroy(metadata->edge_fields.origins);
    metadata->edge_fields.origins = NULL;
}
SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(deinit_node_metadata_edge);

const struct EdgeFieldConstraint *Edge_GetConstraint(unsigned constraint_id) {
    if (constraint_id >= num_elem(edge_constraints)) {
        return NULL;
    }

    return &edge_constraints[constraint_id];
}

const struct EdgeFieldConstraint *Edge_GetFieldConstraint(const struct EdgeField *edge_field) {
    unsigned i = edge_field->constraint_id;

    /* Return the default edge constraint if the id is invalid. */
    return &edge_constraints[i < num_elem(edge_constraints) ? i : 0];
}

static struct EdgeField *new_EdgeField(Selva_NodeId src_node_id, unsigned constraint_id, size_t initial_size) {
    struct EdgeField *edgeField;

    if (constraint_id >= num_elem(edge_constraints)) {
        return NULL;
    }

    edgeField = RedisModule_Calloc(1, sizeof(struct EdgeField));
    if (!edgeField) {
        return NULL;
    }

    edgeField->constraint_id = constraint_id;
    memcpy(edgeField->src_node_id, src_node_id, SELVA_NODE_ID_SIZE);
    SVector_Init(&edgeField->arcs, initial_size, SelvaSVectorComparator_Node);

    return edgeField;
}

struct EdgeField *Edge_GetField(struct SelvaModify_HierarchyNode *src_node, const char *key_name_str, size_t key_name_len) {
    struct SelvaModify_HierarchyMetadata *src_metadata;
    struct EdgeField *src_edge_field;
    int err;

    /* Some callers expect that src_node can be NULL. */
    if (!src_node) {
        return NULL;
    }

    /*
     * The edges object is allocated lazily so the called might need to allocate it.
     */
    src_metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(src_node);
    if (!src_metadata->edge_fields.edges) {
        return NULL;
    }

    err = SelvaObject_GetPointerStr(src_metadata->edge_fields.edges, key_name_str, key_name_len, (void **)(&src_edge_field));
    if (err) {
        return NULL;
    }

    return src_edge_field;
}

static struct EdgeField *Edge_NewField(struct SelvaModify_HierarchyNode *node, const char *key_name_str, size_t key_name_len, unsigned constraint_id) {
    Selva_NodeId node_id;
    struct SelvaModify_HierarchyMetadata *node_metadata;
    struct SelvaObject *edges;
    struct EdgeField *edge_field;

    SelvaModify_HierarchyGetNodeId(node_id, node);
    node_metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(node);

    edges = node_metadata->edge_fields.edges;
    if (!edges) {
        edges = SelvaObject_New();
        if (!edges) {
            return NULL;
        }

        node_metadata->edge_fields.edges = edges;
    }

    edge_field = new_EdgeField(node_id, constraint_id, 0);
    if (!edge_field) {
        /* Just leave the edges obj there as it's already properly initialized. */
        return NULL;
    }

    SelvaObject_SetPointerStr(edges, key_name_str, key_name_len, edge_field, &obj_opts);

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
    dst_node_metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(dst_node);

    if (!dst_node_metadata->edge_fields.origins) {
        /* The edge origin refs struct is initialized lazily. */
        dst_node_metadata->edge_fields.origins = SelvaObject_New();
        /* TODO It could be problematic if we failed to create the object now */
    }
    err = SelvaObject_AddArrayStr(dst_node_metadata->edge_fields.origins, src_edge_field->src_node_id, SELVA_NODE_ID_SIZE, SELVA_OBJECT_POINTER, src_edge_field);
    if (err) {
        /* TODO This error would be pretty fatal now. */
        fprintf(stderr, "%s:%d: Edge origin update failed: %s\n",
                __FILE__, __LINE__,
                getSelvaErrorStr(err));
    }
}

static int get_or_create_EdgeField(struct SelvaModify_HierarchyNode *node, const char *key_name_str, size_t key_name_len, unsigned constraint_id, struct EdgeField **out) {
    struct EdgeField *edge_field;

    edge_field = Edge_GetField(node, key_name_str, key_name_len);
    if (!edge_field) {
        edge_field = Edge_NewField(node, key_name_str, key_name_len, constraint_id);
        if (!edge_field) {
            return SELVA_ENOMEM;
        }
    } else {
        if (edge_field->constraint_id != constraint_id) {
            return SELVA_EINVAL;
        }
        if (SVector_Search(&edge_field->arcs, node)) {
            return SELVA_EEXIST;
        }
    }

    *out = edge_field;
    return 0;
}

int Edge_Has(struct EdgeField *edgeField, struct SelvaModify_HierarchyNode *dst_node) {
    return SVector_SearchIndex(&edgeField->arcs, dst_node) >= 0;
}

int Edge_Add(const char *key_name_str, size_t key_name_len, unsigned constraint_id, struct SelvaModify_HierarchyNode *src_node, struct SelvaModify_HierarchyNode *dst_node) {
    const struct EdgeFieldConstraint *constraint;
    struct EdgeField *src_edge_field;
    int err;

    constraint = Edge_GetConstraint(constraint_id);
    if (!constraint) {
        return SELVA_EINVAL;
    }

    err = get_or_create_EdgeField(src_node, key_name_str, key_name_len, constraint_id, &src_edge_field);
    if (err) {
        return err;
    }

    if (Edge_Has(src_edge_field, dst_node)) {
        return SELVA_EEXIST;
    }

    if (constraint->flags.single_ref) {
        /* single_ref allows only one edge to exist in the field. */
        clear_field(src_edge_field);
    }

    insert_edge(src_edge_field, dst_node);

    return 0;
}

static int remove_origin_ref(struct EdgeField *src_edge_field, struct SelvaModify_HierarchyNode *dst_node) {
    struct SelvaModify_HierarchyMetadata *metadata;
    enum SelvaObjectType out_subtype;
    SVector *origin_fields;
    int err;

    metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(dst_node);
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

int Edge_Delete(struct EdgeField *edgeField, struct SelvaModify_HierarchyNode *src_node, Selva_NodeId dst_node_id) {
    struct EdgeField *src_edge_field = edgeField;
    struct SelvaModify_HierarchyNode *dst_node;
    int err;

    dst_node = SVector_Search(&src_edge_field->arcs, dst_node_id);
    if (!dst_node) {
        return SELVA_ENOENT;
    }
    err = remove_origin_ref(src_edge_field, dst_node);
    if (err) {
        return err;
    }

    /*
     * Delete the edge.
     */
    SVector_Remove(&src_edge_field->arcs, dst_node_id);

    return 0;
}

static void clear_field(struct EdgeField *src_edge_field) {
    SVector *arcs = &src_edge_field->arcs;
    struct SelvaModify_HierarchyNode *dst_node;
    struct SVectorIterator it;

    while ((dst_node = SVector_Pop(&src_edge_field->arcs))) {
        // TODO WTF how to get the field_name here
    }

    SVector_ForeachBegin(&it, arcs);
    while ((dst_node = SVector_Foreach(&it))) {
        int err;

        err = remove_origin_ref(src_edge_field, dst_node);
        if (err) {
            Selva_NodeId dst_node_id;

            /*
             * RFE: This is a sort of a serious error that may lead into a crash
             * later on.
             */
            SelvaModify_HierarchyGetNodeId(dst_node_id, dst_node);
            fprintf(stderr, "%s:%d: Failed to remove an origin reference: %.*s <- %.*s: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, src_edge_field->src_node_id,
                    (int)SELVA_NODE_ID_SIZE, dst_node_id,
                    getSelvaErrorStr(err));
        }
    }

    SVector_Clear(&src_edge_field->arcs);
}

int Edge_ClearField(struct SelvaModify_HierarchyNode *src_node, const char *key_name_str, size_t key_name_len) {
    struct EdgeField *src_edge_field;

    src_edge_field = Edge_GetField(src_node, key_name_str, key_name_len);
    if (!src_edge_field) {
        return SELVA_ENOENT;
    }

    clear_field(src_edge_field);

    return 0;
}

int Edge_DeleteField(struct SelvaModify_HierarchyNode *src_node, const char *key_name_str, size_t key_name_len) {
    struct EdgeField *src_edge_field;

    src_edge_field = Edge_GetField(src_node, key_name_str, key_name_len);
    if (!src_edge_field) {
        return SELVA_ENOENT;
    }

    /*
     * Doing this will cause a full cleanup of the edges and origin pointers (EdgeField_Free()).
     */
    if (SelvaObject_DelKeyStr(SelvaModify_HierarchyGetNodeMetadataByPtr(src_node)->edge_fields.edges, key_name_str, key_name_len)) {
        fprintf(stderr, "%s:%d: Failed to delete the edge field: \"%.*s\"",
                __FILE__, __LINE__,
                (int)key_name_len, key_name_str);
    }

    return 0;
}

int Edge_Refcount(struct SelvaModify_HierarchyNode *node) {
    struct SelvaModify_HierarchyMetadata *metadata;
    int refcount = 0;

    metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(node);

    if (metadata->edge_fields.origins) {
        refcount = SelvaObject_Len(metadata->edge_fields.origins, NULL);
    }

    return refcount;
}

static void EdgeField_Reply(struct RedisModuleCtx *ctx, void *p) {
    struct EdgeField *edge_field = (struct EdgeField *)p;
    SVector *arcs = &edge_field->arcs;
    struct SelvaModify_HierarchyNode *dst_node;
    struct SVectorIterator it;

    RedisModule_ReplyWithArray(ctx, SVector_Size(arcs));
#if 0
    RedisModule_ReplyWithArray(ctx, 1 + SVector_Size(arcs));
    RedisModule_ReplyWithLongLong(ctx, edge_field->constraint_id);
#endif

    SVector_ForeachBegin(&it, arcs);
    while ((dst_node = SVector_Foreach(&it))) {
        Selva_NodeId dst_node_id;

        SelvaModify_HierarchyGetNodeId(dst_node_id, dst_node);
        RedisModule_ReplyWithStringBuffer(ctx, dst_node_id, Selva_NodeIdLen(dst_node_id));
    }
}

static void EdgeField_Free(void *p) {
    struct EdgeField *edge_field = (struct EdgeField *)p;

    clear_field(edge_field);
    SVector_Destroy(&edge_field->arcs);
    RedisModule_Free(p);
}

static size_t EdgeField_Len(void *p) {
    struct EdgeField *edge_field = (struct EdgeField *)p;

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
    size_t nr_edges;
    struct EdgeField *edge_field;
    int err;

    constraint_id = RedisModule_LoadUnsigned(io);
    nr_edges = RedisModule_LoadUnsigned(io);

    SelvaModify_HierarchyGetNodeId(src_node_id, load_data->src_node);
    edge_field = new_EdgeField(src_node_id, constraint_id, nr_edges);
    if (!edge_field) {
        return NULL;
    }

    for (size_t i = 0; i < nr_edges; i++) {
        RedisModuleString *dst_id;
        struct SelvaModify_HierarchyNode *dst_node;

        dst_id = RedisModule_LoadString(io);
        TO_STR(dst_id);

        if (dst_id_len != SELVA_NODE_ID_SIZE) {
            return NULL;
        }

        /*
         * Ensure that the destination node exist before creating an edge.
         */
        err = SelvaModify_AddHierarchy(ctx, hierarchy, dst_id_str, 0, NULL, 0, NULL);
        if (err) {
            RedisModule_LogIOError(io, "warning", "AddHierarchy() failed");
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

    if (encver < HIERARCHY_ENCODING_VERSION) {
        return 0; /* Only the latest version supports loading metadata. */
    }

    if (unlikely(!ctx)) {
        RedisModule_LogIOError(io, "warning", "Redis ctx can't be NULL");
        return SELVA_EINVAL;
    }

    /* A boolean flag to tell whether there are any edge fields. */
    if (RedisModule_LoadUnsigned(io)) {
        /*
         * We use the SelvaObject RDB loader to load the object which will then
         * call EdgeField_RdbLoad for each field stored in the object to
         * initialize the actual EdgeField structures.
         */
        SelvaObjectTypeRDBLoad(io, encver, &(struct EdgeField_load_data){
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
    struct EdgeField *edgeField = (struct EdgeField *)value;
    struct SVectorIterator vec_it;
    struct SelvaModify_HierarchyNode *dst_node;

    RedisModule_SaveUnsigned(io, edgeField->constraint_id);
    RedisModule_SaveUnsigned(io, SVector_Size(&edgeField->arcs)); /* nr_edges */

    SVector_ForeachBegin(&vec_it, &edgeField->arcs);
    while ((dst_node = SVector_Foreach(&vec_it))) {
        Selva_NodeId dst_node_id;

        SelvaModify_HierarchyGetNodeId(dst_node_id, dst_node);
        RedisModule_SaveStringBuffer(io, dst_node_id, SELVA_NODE_ID_SIZE);
    }
}

void Edge_RdbSave(struct RedisModuleIO *io, struct SelvaModify_HierarchyNode *node) {
    struct SelvaModify_HierarchyMetadata *metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(node);
    struct SelvaObject *edges = metadata->edge_fields.edges;

    /* A boolean marker to trigger the loader. */
    if (edges && SelvaObject_Len(edges, NULL) > 0) {
        RedisModule_SaveUnsigned(io, 1);
        SelvaObjectTypeRDBSave(io, metadata->edge_fields.edges, NULL);
    } else {
        RedisModule_SaveUnsigned(io, 0);
    }
}
