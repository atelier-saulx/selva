#include <stddef.h>
#include "redismodule.h"
#include "errors.h"
#include "edge.h"
#include "svector.h"
#include "selva_object.h"
#include "hierarchy.h"

struct EdgeFieldConstraint {
    struct {
        unsigned is_directed : 1;
        unsigned action_delete_node : 1;    /*!< Delete the node when the edge field is empty. */
    } flags;
};

static void EdgeField_Reply(struct RedisModuleCtx *ctx, void *p);
static void EdgeField_Free(void *p);
static size_t EdgeField_Len(void *p);
static void *EdgeField_RdbLoad(struct RedisModuleIO *io, int encver, void *data);
static void EdgeField_RdbSave(struct RedisModuleIO *io, void *value, void *data);

static const struct EdgeFieldConstraint edge_constraints[] = {
    {
        .flags = {
            .is_directed = 1,
            .action_delete_node = 0,
        },
    },
};

static const struct SelvaObjectPointerOpts obj_opts = {
    .ptr_type_id = 1, /* TODO reserve these in a nice way */
    .ptr_reply = EdgeField_Reply,
    .ptr_free = EdgeField_Free,
    .ptr_len = EdgeField_Len,
    .ptr_save = EdgeField_RdbSave,
    .ptr_load = EdgeField_RdbLoad,
};

static int SVector_Edge_cmp(const void ** restrict a, const void ** restrict b) {
    return memcmp(*a, *b, SELVA_NODE_ID_SIZE);
}

static void init_node_metadata_edge(
        Selva_NodeId id __unused,
        struct SelvaModify_HierarchyMetadata *metadata) {
    metadata->custom_edge_fields.edges = NULL;
    metadata->custom_edge_fields.origins = NULL;
}
SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(init_node_metadata_edge);

static void deinit_node_metadata_edge(const Selva_NodeId node_id, struct SelvaModify_HierarchyMetadata *metadata) {
    struct SelvaObject *origins;
    void *obj_it;
    SVector *edge_fields;
    const char *src_node_id;

    origins = metadata->custom_edge_fields.origins;
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
            SVector_Remove(&src_field->edges, (void *)node_id);
        }
    }

    SelvaObject_Destroy(metadata->custom_edge_fields.origins);
    metadata->custom_edge_fields.origins = NULL;
}
SELVA_MODIFY_HIERARCHY_METADATA_DESTRUCTOR(deinit_node_metadata_edge);

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
    SVector_Init(&edgeField->edges, initial_size, SVector_Edge_cmp);

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
    if (!src_metadata->custom_edge_fields.edges) {
        return NULL;
    }

    err = SelvaObject_GetPointerStr(src_metadata->custom_edge_fields.edges, key_name_str, key_name_len, (void **)(&src_edge_field));
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

    edges = node_metadata->custom_edge_fields.edges;
    if (!edges) {
        edges = SelvaObject_New();
        if (!edges) {
            return NULL;
        }

        node_metadata->custom_edge_fields.edges = edges;
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
    SVector_InsertFast(&src_edge_field->edges, dst_node);

    /*
     * Add origin reference.
     * Note that we must ensure that this insertion is only ever done once.
     */
    dst_node_metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(dst_node);

    if (!dst_node_metadata->custom_edge_fields.origins) {
        /* The edge origin refs struct is initialized lazily. */
        dst_node_metadata->custom_edge_fields.origins = SelvaObject_New();
        /* TODO It could be problematic if we failed to create the object now */
    }
    err = SelvaObject_AddArrayStr(dst_node_metadata->custom_edge_fields.origins, src_edge_field->src_node_id, SELVA_NODE_ID_SIZE, SELVA_OBJECT_POINTER, src_edge_field);
    if (err) {
        /* TODO This error would be pretty fatal now. */
        fprintf(stderr, "%s:%d: Edge origin update failed: %s\n",
                __FILE__, __LINE__,
                getSelvaErrorStr(err));
    }
}

int Edge_Add(const char *key_name_str, size_t key_name_len, unsigned constraint_id, struct SelvaModify_HierarchyNode *src_node, struct SelvaModify_HierarchyNode *dst_node) {
    struct EdgeField *src_edge_field;

    src_edge_field = Edge_GetField(src_node, key_name_str, key_name_len);
    if (!src_edge_field) {
        src_edge_field = Edge_NewField(src_node, key_name_str, key_name_len, constraint_id);
        if (!src_edge_field) {
            return SELVA_ENOMEM;
        }
    } else if (src_edge_field->constraint_id != constraint_id) {
        return SELVA_EINVAL;
    }

    if (SVector_Search(&src_edge_field->edges, dst_node)) {
        return SELVA_EEXIST;
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
    err = SelvaObject_GetArrayStr(metadata->custom_edge_fields.origins, src_edge_field->src_node_id, SELVA_NODE_ID_SIZE, &out_subtype, &origin_fields);
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
        SelvaObject_DelKeyStr(metadata->custom_edge_fields.origins, src_edge_field->src_node_id, SELVA_NODE_ID_SIZE);
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

int Edge_Delete(const char *key_name_str, size_t key_name_len, struct SelvaModify_HierarchyNode *src_node, Selva_NodeId dst_node_id) {
    struct EdgeField *src_edge_field;
    struct SelvaModify_HierarchyNode *dst_node;
    int err;

    src_edge_field = Edge_GetField(src_node, key_name_str, key_name_len);
    if (!src_edge_field) {
        return SELVA_ENOENT;
    }

    /* This works because nodes start with the nodeId. */
    dst_node = SVector_Search(&src_edge_field->edges, dst_node_id);
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
    SVector_Remove(&src_edge_field->edges, dst_node_id);

    return 0;
}

static void clear_field(struct EdgeField *src_edge_field) {
    struct SelvaModify_HierarchyNode *dst_node;
    struct SVectorIterator it;

    SVector_ForeachBegin(&it, &src_edge_field->edges);
    while ((dst_node = SVector_Foreach(&it))) {
        int err;

        err = remove_origin_ref(src_edge_field, dst_node);
        if (err) {
            Selva_NodeId dst_node_id;

            SelvaModify_HierarchyGetNodeId(dst_node_id, dst_node);
            fprintf(stderr, "%s:%d: Failed to remove an origin reference: source: %.*s <-> destination: %.*s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, src_edge_field->src_node_id,
                    (int)SELVA_NODE_ID_SIZE, dst_node_id);
        }
    }

    SVector_Clear(&src_edge_field->edges);
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
    if (SelvaObject_DelKeyStr(SelvaModify_HierarchyGetNodeMetadataByPtr(src_node)->custom_edge_fields.edges, key_name_str, key_name_len)) {
        fprintf(stderr, "%s:%d: Failed to delete the edge field: \"%.*s\"",
                __FILE__, __LINE__,
                (int)key_name_len, key_name_str);
    }

    return 0;
}

static void EdgeField_Reply(struct RedisModuleCtx *ctx, void *p) {
    struct EdgeField *edge_field = (struct EdgeField *)p;
    SVector *edges = &edge_field->edges;
    struct SelvaModify_HierarchyNode *dst_node;
    struct SVectorIterator it;

    RedisModule_ReplyWithArray(ctx, SVector_Size(edges));
#if 0
    RedisModule_ReplyWithArray(ctx, 1 + SVector_Size(&edge_field->edges));
    RedisModule_ReplyWithLongLong(ctx, edge_field->constraint_id);
#endif

    SVector_ForeachBegin(&it, edges);
    while ((dst_node = SVector_Foreach(&it))) {
        Selva_NodeId dst_node_id;

        SelvaModify_HierarchyGetNodeId(dst_node_id, dst_node);
        RedisModule_ReplyWithStringBuffer(ctx, dst_node_id, Selva_NodeIdLen(dst_node_id));
    }
}

static void EdgeField_Free(void *p) {
    struct EdgeField *edge_field = (struct EdgeField *)p;

    clear_field(edge_field);
    SVector_Destroy(&edge_field->edges);
    RedisModule_Free(p);
}

static size_t EdgeField_Len(void *p) {
    struct EdgeField *edge_field = (struct EdgeField *)p;

    return SVector_Size(&edge_field->edges);
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
    RedisModule_SaveUnsigned(io, SVector_Size(&edgeField->edges)); /* nr_edges */

    SVector_ForeachBegin(&vec_it, &edgeField->edges);
    while ((dst_node = SVector_Foreach(&vec_it))) {
        Selva_NodeId dst_node_id;

        SelvaModify_HierarchyGetNodeId(dst_node_id, dst_node);
        RedisModule_SaveStringBuffer(io, dst_node_id, SELVA_NODE_ID_SIZE);
    }
}

void Edge_RdbSave(struct RedisModuleIO *io, struct SelvaModify_HierarchyNode *node) {
    struct SelvaModify_HierarchyMetadata *metadata = SelvaModify_HierarchyGetNodeMetadataByPtr(node);
    struct SelvaObject *edges = metadata->custom_edge_fields.edges;

    /* A boolean marker to trigger the loader. */
    if (edges && SelvaObject_Len(edges, NULL) > 0) {
        RedisModule_SaveUnsigned(io, 1);
        SelvaObjectTypeRDBSave(io, metadata->custom_edge_fields.edges, NULL);
    } else {
        RedisModule_SaveUnsigned(io, 0);
    }
}
