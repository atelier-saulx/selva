/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include "alloca.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "jemalloc.h"
#include "util/auto_free.h"
#include "util/ctime.h"
#include "util/selva_string.h"
#include "util/svector.h"
#include "util/timestamp.h"
#include "selva_error.h"
#include "selva_server.h"
#include "selva_db.h"
#include "selva_io.h"
#include "selva_log.h"
#include "arg_parser.h"
#include "async_task.h"
#include "edge.h"
#include "modify.h"
#include "rpn.h"
#include "config.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_trace.h"
#include "find_index.h"
#include "selva_set.h"
#include "subscriptions.h"
#include "traversal.h"
#include "hierarchy.h"
#include "hierarchy_detached.h"
#include "hierarchy_inactive.h"

/**
 * Selva module version tracking.
 * This is used to track the Selva module version used to create and modify the
 * hierarchy that was serialized and later deserialized.
 */
static struct SelvaDbVersionInfo {
    const struct selva_string *created_with;
    const struct selva_string *updated_with;
} selva_db_version_info;

/**
 * Node flags changing the node behavior.
 */
enum SelvaNodeFlags {
    /**
     * Detached node.
     * When set this is the head of a compressed subtree stored in
     * hierarchy_detached. Some information has been removed from the node
     * and the subtree must be restored to make this node usable.
     */
    SELVA_NODE_FLAGS_DETACHED = 0x01,
    /**
     * Implicitly created node.
     * Nodes that are created through child or references lists are implicit.
     * The flag should be cleared when the node is actually taken into use.
     */
    SELVA_NODE_FLAGS_IMPLICIT = 0x02,
} __packed;

/**
 * The core type of Selva hierarchy.
 */
typedef struct SelvaHierarchyNode {
    Selva_NodeId id; /* Must be first. */
    enum SelvaNodeFlags flags;
    struct trx trx_label;
#if HIERARCHY_SORT_BY_DEPTH
    ssize_t depth;
#endif
    STATIC_SELVA_OBJECT(_obj_data);
    struct SelvaHierarchyMetadata metadata;
    SVector parents;
    SVector children;
    RB_ENTRY(SelvaHierarchyNode) _index_entry;
} SelvaHierarchyNode;

/**
 * Filter struct used for RB searches from hierarchy_index_tree.
 * This should somewhat match to SelvaHierarchyNode to the level necessary for
 * comparing nodes.
 */
struct SelvaHierarchySearchFilter {
    Selva_NodeId id;
};

/**
 * Hierarchy ancestral relationship types.
 */
enum SelvaHierarchyNode_Relationship {
    RELATIONSHIP_PARENT,
    RELATIONSHIP_CHILD,
};

#define GET_NODE_OBJ(_node_) \
    ((struct SelvaObject *)((_node_)->_obj_data))

/**
 * Structure for traversal cb of verifyDetachableSubtree().
 */
struct verifyDetachableSubtree {
    const char *err; /*!< Set to a reason string if subtree doesn't verify. */
    struct trx trx_cur; /*!< This is used to check if the children of node form a true subtree. */
    SelvaHierarchyNode *head;
    struct SelvaSet edge_origin_node_ids;
};

/**
 * Type used by subtree serialization traversal functions.
 */
struct SelvaHierarchySubtree {
    SelvaHierarchy *hierarchy;
    struct SelvaHierarchyNode *node;
};

/**
 * HierarchyRDBSaveNode() args struct.
 */
struct HierarchyRDBSaveNode {
    struct selva_io *io;
};

static void SelvaModify_DestroyNode(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node);
static int removeRelationships(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        enum SelvaHierarchyNode_Relationship rel);
RB_PROTOTYPE_STATIC(hierarchy_index_tree, SelvaHierarchyNode, _index_entry, SelvaHierarchyNode_Compare)
/* FIXME Implement subtree compression and detaching */
static int detach_subtree(SelvaHierarchy *hierarchy, struct SelvaHierarchyNode *node, enum SelvaHierarchyDetachedType type);
static int restore_subtree(SelvaHierarchy *hierarchy, const Selva_NodeId id);
static void auto_compress_proc(struct RedisModuleCtx *ctx, void *data);

/* Node metadata constructors. */
SET_DECLARE(selva_HMCtor, SelvaHierarchyMetadataConstructorHook);
/* Node metadata destructors. */
SET_DECLARE(selva_HMDtor, SelvaHierarchyMetadataDestructorHook);

__nonstring static const Selva_NodeId HIERARCHY_RDB_EOF;
/* FIXME */
#if 0
static RedisModuleType *HierarchyType;
static RedisModuleType *HierarchySubtreeType;
#endif

SelvaHierarchy *main_hierarchy;

SELVA_TRACE_HANDLE(find_inmem);
SELVA_TRACE_HANDLE(find_detached);
SELVA_TRACE_HANDLE(restore_subtree);
/* FIXME */
#if 0
SELVA_TRACE_HANDLE(auto_compress_proc);
#endif

/**
 * A pointer to the hierarchy subtree being loaded.
 * Redis doesn't allow passing any pointers when loading a stringified RDB so a
 * global variable is needed for Hierarchy_SubtreeRDBLoad().
 */
static SelvaHierarchy *subtree_hierarchy;
static int isDecompressingSubtree;

/**
 * Are we executing an RDB save.
 * TODO This should be technically per hierarchy structure.
 */
static int isRdbSaving;

static int isRdbLoading(void) {
     /* FIXME */
#if 0
     return !!(REDISMODULE_CTX_FLAGS_LOADING & RedisModule_GetContextFlags(ctx) || isDecompressingSubtree);
#endif
     return 0;
}

/**
 * Returns 1 on both the RDB process and the parent if we are currently
 * executing an RDB save.
 */
static int isRdbChildRunning(void) {
    /* FIXME */
#if 0
    const int ctx_flags = RedisModule_GetContextFlags(ctx);

    /*
     * We also want to know if there is an ongoing save. Redis doesn't provide such
     * information but we can try to guess by checking if we have a child process,
     * as typically a child process should be the RDB process.
     */
    return (ctx_flags & (REDISMODULE_CTX_FLAGS_RDB)) &&
           (ctx_flags & (REDISMODULE_CTX_FLAGS_ACTIVE_CHILD | REDISMODULE_CTX_FLAGS_IS_CHILD));
#endif

    return 0;
}

static int SVector_HierarchyNode_id_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const SelvaHierarchyNode *a = *(const SelvaHierarchyNode **)a_raw;
    const SelvaHierarchyNode *b = *(const SelvaHierarchyNode **)b_raw;

    assert(a);
    assert(b);

    return memcmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

#if HIERARCHY_SORT_BY_DEPTH
static int SVector_HierarchyNode_depth_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const SelvaHierarchyNode *a = *(const SelvaHierarchyNode **)a_raw;
    const SelvaHierarchyNode *b = *(const SelvaHierarchyNode **)b_raw;

    assert(a);
    assert(b);

    return b->depth - a->depth;
}
#endif

static int SelvaHierarchyNode_Compare(const SelvaHierarchyNode *a, const SelvaHierarchyNode *b) {
    return memcmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

RB_GENERATE_STATIC(hierarchy_index_tree, SelvaHierarchyNode, _index_entry, SelvaHierarchyNode_Compare)

SelvaHierarchy *SelvaModify_NewHierarchy(void) {
    SelvaHierarchy *hierarchy = selva_calloc(1, sizeof(*hierarchy));

    mempool_init(&hierarchy->node_pool, HIERARCHY_SLAB_SIZE, sizeof(SelvaHierarchyNode), _Alignof(SelvaHierarchyNode));
    RB_INIT(&hierarchy->index_head);
    SVector_Init(&hierarchy->heads, 1, SVector_HierarchyNode_id_compare);
    SelvaObject_Init(hierarchy->types._obj_data);
    SelvaObject_Init(hierarchy->aliases._obj_data);
    Edge_InitEdgeFieldConstraints(&hierarchy->edge_field_constraints);
    SelvaSubscriptions_InitHierarchy(hierarchy);
    SelvaFindIndex_Init(hierarchy);

    if (SelvaModify_SetHierarchy(hierarchy, ROOT_NODE_ID, 0, NULL, 0, NULL, NULL) < 0) {
        SelvaModify_DestroyHierarchy(hierarchy);
        hierarchy = NULL;
        goto fail;
    }

    /* FIXME auto compress */
#if 0
    if (selva_glob_config.hierarchy_auto_compress_period_ms > 0) {
        if (SelvaHierarchy_InitInactiveNodes(hierarchy, HIERARCHY_AUTO_COMPRESS_INACT_NODES_LEN)) {
            SelvaModify_DestroyHierarchy(hierarchy);
            hierarchy = NULL;
            goto fail;
        }

        hierarchy->inactive.auto_compress_timer = RedisModule_CreateTimer(ctx, selva_glob_config.hierarchy_auto_compress_period_ms, auto_compress_proc, hierarchy);
    }
#endif

fail:
    return hierarchy;
}

void SelvaModify_DestroyHierarchy(SelvaHierarchy *hierarchy) {
    SelvaHierarchyNode *node;
    SelvaHierarchyNode *next;

    for (node = RB_MIN(hierarchy_index_tree, &hierarchy->index_head); node != NULL; node = next) {
        next = RB_NEXT(hierarchy_index_tree, &hierarchy->index_head, node);
        RB_REMOVE(hierarchy_index_tree, &hierarchy->index_head, node);
        SelvaModify_DestroyNode(hierarchy, node);
    }

    SelvaSubscriptions_DestroyAll(hierarchy);
    /*
     * If SelvaSubscriptions_DestroyAll() is ran first the we don't need to
     * bother about cleaning up subscriptions used by the indexing and thus
     * hopefully the deinit doesn't need a RedisModuleCtx.
     */
    SelvaFindIndex_Deinit(hierarchy);

    Edge_DeinitEdgeFieldConstraints(&hierarchy->edge_field_constraints);

    SVector_Destroy(&hierarchy->heads);

    if (hierarchy->inactive.nr_nodes) {
        /* FIXME inact timer auto compress */
#if 0
        (void)RedisModule_StopTimerUnsafe(hierarchy->inactive.auto_compress_timer, NULL);
#endif
    }
    /* FIXME Implement inact nodes */
#if 0
    SelvaHierarchy_DeinitInactiveNodes(hierarchy);
#endif
    mempool_destroy(&hierarchy->node_pool);

#if MEM_DEBUG
    memset(hierarchy, 0, sizeof(*hierarchy));
#endif
    selva_free(hierarchy);
}

/**
 * Create the default fields of a node object.
 * This function should be called when creating a new node but not when loading
 * nodes from an RDB data.
 */
static int create_node_object(struct SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    const long long now = ts_now();
    struct SelvaObject *obj;
    struct selva_string *node_name;
    int err;

    node_name = selva_string_createf("%.*s", (int)SELVA_NODE_ID_SIZE, node->id);
    obj = SelvaObject_Init(node->_obj_data);

    err = SelvaObject_SetStringStr(obj, SELVA_ID_FIELD, sizeof(SELVA_ID_FIELD) - 1, node_name);
    if (err) {
        selva_string_free(node_name);
        return err;
    }

    /* Set the type for root. */
    if (!memcmp(node->id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE)) {
        struct selva_string *type;

        type = selva_string_create("root", 4, 0);
        err = SelvaObject_SetStringStr(obj, SELVA_TYPE_FIELD, sizeof(SELVA_TYPE_FIELD) - 1, type);
        if (err) {
            selva_string_free(type);
            return err;
        }
    } else {
        struct selva_string *type;

        type = SelvaHierarchyTypes_Get(hierarchy, node->id);
        if (type) {
            err = SelvaObject_SetStringStr(obj, SELVA_TYPE_FIELD, sizeof(SELVA_TYPE_FIELD) - 1, type);
            if (err) {
                return err;
            }
        }
    }

    SelvaObject_SetLongLongStr(obj, SELVA_UPDATED_AT_FIELD, sizeof(SELVA_UPDATED_AT_FIELD) - 1, now);
    SelvaObject_SetLongLongStr(obj, SELVA_CREATED_AT_FIELD, sizeof(SELVA_CREATED_AT_FIELD) - 1, now);

    return 0;
}

/*
 * This function is not really necessary but we have it to make sure that
 * the metadata constructor linker set is always created.
 */
static void node_metadata_init(
        const Selva_NodeId id __unused,
        struct SelvaHierarchyMetadata *metadata __unused) {
    /* NOP */
}
SELVA_MODIFY_HIERARCHY_METADATA_CONSTRUCTOR(node_metadata_init);

/**
 * Create a new node.
 */
static SelvaHierarchyNode *newNode(struct SelvaHierarchy *hierarchy, const Selva_NodeId id) {
    SelvaHierarchyNode *node;

    if (!memcmp(id, EMPTY_NODE_ID, SELVA_NODE_ID_SIZE) ||
        !memcmp(id, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE)) {
        SELVA_LOG(SELVA_LOGL_WARN, "An attempt to create a node with a reserved id");
        return NULL;
    }

    node = mempool_get(&hierarchy->node_pool);
    memset(node, 0, sizeof(*node));

#if 0
    fprintf(stderr, "%s:%d: Creating node %.*s\n",
            __FILE__, __LINE__,
            (int)SELVA_NODE_ID_SIZE, id);
#endif

    memcpy(node->id, id, SELVA_NODE_ID_SIZE);
    SVector_Init(&node->parents,  selva_glob_config.hierarchy_initial_vector_len, SVector_HierarchyNode_id_compare);
    SVector_Init(&node->children, selva_glob_config.hierarchy_initial_vector_len, SVector_HierarchyNode_id_compare);

    /* The SelvaObject is created elsewhere if we are loading. */
    if (likely(!isRdbLoading())) {
        int err;

        err = create_node_object(hierarchy, node);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to create a node object for \"%.*s\": %s\n",
                      (int)SELVA_NODE_ID_SIZE, id,
                      selva_strerror(err));
        }

        /*
         * Every node is implicit unless it isn't. Modify should clear this flag
         * when explicitly creating a node, that can happen on a later command
         * call. This flag will be also persisted in the RDB.
         */
        node->flags |= SELVA_NODE_FLAGS_IMPLICIT;
    }

    SelvaHierarchyMetadataConstructorHook **metadata_ctor_p;

    SET_FOREACH(metadata_ctor_p, selva_HMCtor) {
        SelvaHierarchyMetadataConstructorHook *ctor = *metadata_ctor_p;
        ctor(node->id, &node->metadata);
    }

    return node;
}

/**
 * Actions that must be executed for a new node.
 * Generally this must be always called after newNode() unless we are RDB loading.
 */
static void new_node_events(SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
        SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node, SELVA_CREATED_AT_FIELD, sizeof(SELVA_CREATED_AT_FIELD) - 1);
        SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node, SELVA_UPDATED_AT_FIELD, sizeof(SELVA_UPDATED_AT_FIELD) - 1);
        SelvaSubscriptions_DeferMissingAccessorEvents(hierarchy, node->id, SELVA_NODE_ID_SIZE);
}

static void SelvaModify_DestroyNode(SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    SelvaHierarchyMetadataDestructorHook **dtor_p;

    SET_FOREACH(dtor_p, selva_HMDtor) {
        SelvaHierarchyMetadataDestructorHook *dtor = *dtor_p;
        dtor(hierarchy, node, &node->metadata);
    }

    SVector_Destroy(&node->parents);
    SVector_Destroy(&node->children);
    SelvaObject_Destroy(GET_NODE_OBJ(node));
#if MEM_DEBUG
    memset(node, 0, sizeof(*node));
#endif
    mempool_return(&hierarchy->node_pool, node);
}

/**
 * Create a new detached node with given parents.
 */
static void new_detached_node(SelvaHierarchy *hierarchy, const Selva_NodeId node_id, Selva_NodeId *parents, size_t nr_parents) {
    const int prevIsDecompressingSubtree = isDecompressingSubtree;
    struct SelvaHierarchyNode *node;
    int err;

    /*
     * We are not actually decompressing but we need to make it look like we are.
     */
    isDecompressingSubtree = 1;
    err = SelvaHierarchy_UpsertNode(hierarchy, node_id, &node);
    isDecompressingSubtree = prevIsDecompressingSubtree;

    if (!err) {
        err = SelvaModify_AddHierarchyP(hierarchy, node, nr_parents, parents, 0, NULL);
        node->flags |= SELVA_NODE_FLAGS_DETACHED;
        SelvaObject_Destroy(GET_NODE_OBJ(node));
    }

    if (unlikely(err < 0)) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Fatal error while creating a detached node %.*s: %s\n",
                  (int)SELVA_NODE_ID_SIZE, node_id,
                  selva_strerror(err));
        abort();
    }
}

/**
 * Reinit everything that was removed when the subtree head was made detached.
 * There should be no need to ever call this function from anywhere else but
 * SelvaHierarchy_FindNode().
 */
static int repopulate_detached_head(struct SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    int err;

    err = create_node_object(hierarchy, node);
    if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to repopulate a detached dummy node %.*s\n",
                  (int)SELVA_NODE_ID_SIZE, node->id);
        return err;
    }

    node->flags &= ~SELVA_NODE_FLAGS_DETACHED;

    return 0;
}

/**
 * Search from the normal node index.
 * This function doesn't decompress subtrees nor checks if the node exists in
 * the detached node index.
 */
static SelvaHierarchyNode *find_node_index(SelvaHierarchy *hierarchy, const Selva_NodeId id) {
    struct SelvaHierarchySearchFilter filter;
    SelvaHierarchyNode *node;

    memcpy(&filter.id, id, SELVA_NODE_ID_SIZE);

    SELVA_TRACE_BEGIN(find_inmem);
    node = RB_FIND(hierarchy_index_tree, &hierarchy->index_head, (SelvaHierarchyNode *)(&filter));
    SELVA_TRACE_END(find_inmem);

    return node;
}

SelvaHierarchyNode *SelvaHierarchy_FindNode(SelvaHierarchy *hierarchy, const Selva_NodeId id) {
    int err;

    {
        /* We want to reduce the scope of `node` for dev safety. */
        SelvaHierarchyNode *node;

        node = find_node_index(hierarchy, id);
        if (node) {
            if (!(node->flags & SELVA_NODE_FLAGS_DETACHED)) {
                return node;
            } else if (isDecompressingSubtree) {
                err = repopulate_detached_head(hierarchy, node);
                if (err) {
                    return NULL;
                }

                return node;
            }
        }
    }

    /*
     * We don't want upsert to be looking from detached nodes.
     * If isDecompressingSubtree is set it means that restore_subtree() was
     * already called once.
     */
    if (SelvaHierarchyDetached_IndexExists(hierarchy) && !isDecompressingSubtree) {
        SELVA_TRACE_BEGIN(find_detached);
        err = restore_subtree(hierarchy, id);
        SELVA_TRACE_END(find_detached);
        if (err) {
            if (err != SELVA_ENOENT && err != SELVA_HIERARCHY_ENOENT) {
                SELVA_LOG(SELVA_LOGL_ERR, "Restoring a subtree containing %.*s failed: %s\n",
                          (int)SELVA_NODE_ID_SIZE, id,
                          selva_strerror(err));
            }

            return NULL;
        }

        return find_node_index(hierarchy, id);
    }

    return NULL;
}

struct SelvaObject *SelvaHierarchy_GetNodeObject(const struct SelvaHierarchyNode *node) {
    return GET_NODE_OBJ(node);
}

const struct SelvaHierarchyMetadata *_SelvaHierarchy_GetNodeMetadataByConstPtr(const SelvaHierarchyNode *node) {
    return &node->metadata;
}

struct SelvaHierarchyMetadata *_SelvaHierarchy_GetNodeMetadataByPtr(SelvaHierarchyNode *node) {
    return &node->metadata;
}

struct SelvaHierarchyMetadata *SelvaHierarchy_GetNodeMetadata(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id) {
    SelvaHierarchyNode *node;

    node = SelvaHierarchy_FindNode(hierarchy, id);

    return !node ? NULL : &node->metadata;
}

static const char * const excluded_fields[] = {
    SELVA_ID_FIELD,
    SELVA_TYPE_FIELD,
    SELVA_CREATED_AT_FIELD,
    SELVA_ALIASES_FIELD,
    NULL
};

int SelvaHierarchy_ClearNodeFlagImplicit(SelvaHierarchyNode *node) {
    const int v = !!(node->flags & SELVA_NODE_FLAGS_IMPLICIT);

    if (v) {
        node->flags &= ~SELVA_NODE_FLAGS_IMPLICIT;
    }

    return v;
}

void SelvaHierarchy_ClearNodeFields(struct SelvaObject *obj) {
    SelvaObject_Clear(obj, excluded_fields);
}

static inline void mkHead(SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    (void)SVector_InsertFast(&hierarchy->heads, node);
}

static inline void rmHead(SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    /* Root should be never removed from heads. */
    if (memcmp(node->id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE)) {
        SVector_Remove(&hierarchy->heads, node);
    }
}

/**
 * Delete all aliases from the aliases key.
 * Note that this function doesn't delete the aliases from the node object.
 */
static void delete_node_aliases(SelvaHierarchy *hierarchy, struct SelvaObject *obj) {
    struct SelvaSet *node_aliases_set;

    node_aliases_set = SelvaObject_GetSetStr(obj, SELVA_ALIASES_FIELD, sizeof(SELVA_ALIASES_FIELD) - 1);
    if (node_aliases_set) {
        delete_aliases(hierarchy, node_aliases_set);
    }
}

static void del_node(SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    const int send_events = !isRdbLoading();
    struct SelvaObject *obj = GET_NODE_OBJ(node);
    Selva_NodeId id;
    int is_root;
    int err;

    memcpy(id, node->id, SELVA_NODE_ID_SIZE);
    is_root = !memcmp(id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE);

    if (send_events) {
        SelvaSubscriptions_DeferTriggerEvents(hierarchy, node, SELVA_SUBSCRIPTION_TRIGGER_TYPE_DELETED);
    }

    err = removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);
    if (err < 0) {
        /* Presumably bad things could happen if we'd proceed now. */
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to remove node parent relationships\n");
        return;
    }
    if (send_events) {
        SelvaSubscriptions_DeferHierarchyDeletionEvents(hierarchy, node);
    }

    delete_node_aliases(hierarchy, obj);

    /*
     * Never delete the root node.
     */
    if (is_root) {
        SelvaHierarchy_ClearNodeFields(obj);

        /*
         * There might be something to collect if this was a large hierarchy.
         * Regardless, running the gc is a relatively cheap operation and makes
         * sense here.
         */
        mempool_gc(&hierarchy->node_pool);
    } else {
        err = removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);
        if (err < 0) {
            /*
             * Well, this is embarassing. The caller won't be happy about
             * having a half-deleted node dangling there.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to remove node child relationships\n");
            return;
        }

        /*
         * The node was now marked as a head but we are going to get rid of it
         * soon, so there is no reason to make it a tree head. In fact, doing
         * so would break things.
         */
        rmHead(hierarchy, node);

        RB_REMOVE(hierarchy_index_tree, &hierarchy->index_head, node);
        SelvaModify_DestroyNode(hierarchy, node);
    }
}

#if HIERARCHY_SORT_BY_DEPTH
static void updateDepth(SelvaHierarchy *hierarchy, SelvaHierarchyNode *head) {
    SVector q;

    if (unlikely(isRdbLoading())) {
        /*
         * Skip updates for now as it would require a full BFS pass for every new node.
         */
        return;
    }

    SVector_Init(&q, selva_glob_config.hierarchy_expected_resp_len, NULL);

    Trx_Begin(&hierarchy->current_trx);
    Trx_Stamp(&hierarchy->current_trx, &head->visit_stamp);
    (void)SVector_InsertFast(&q, head);

    while (SVector_Size(&q) > 0) {
        SelvaHierarchyNode *node = SVector_Shift(&q);
        struct SVectorIterator it;

        /*
         * Update the depth.
         */
        ssize_t new_depth = 0;
        SelvaHierarchyNode *parent;
        SVector_ForeachBegin(&it, &node->parents);
        while ((parent = SVector_Foreach(&it))) {
            new_depth = max(new_depth, parent->depth + 1);
        }
        node->depth = new_depth;

        SelvaHierarchyNode *child;
        SVector_ForeachBegin(&it, &node->children);
        while ((child = SVector_Foreach(&it))) {
            if (!Trx_IsStamped(&hierarchy->current_trx, &child->visit_stamp)) {
                Trx_Stamp(&hierarchy->current_trx, &child->visit_stamp);
                SVector_Insert(&q, child);
            }
        }
    }

    SVector_Destroy(&q);
    Trx_End(&hierarchy->current_trx);
}
#endif

#if HIERARCHY_SORT_BY_DEPTH
ssize_t SelvaModify_GetHierarchyDepth(SelvaHierarchy *hierarchy, const Selva_NodeId id) {
    const SelvaHierarchyNode *node;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return -1;
    }

    return node->depth;
}
#endif

static inline void publishAncestorsUpdate(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node) {
    if (!isRdbLoading()) {
        const char *field_str = SELVA_ANCESTORS_FIELD;
        const size_t field_len = sizeof(SELVA_ANCESTORS_FIELD) - 1;

        SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node, field_str, field_len);
    }
}

static inline void publishDescendantsUpdate(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node) {
    if (!isRdbLoading()) {
        const char *field_str = SELVA_DESCENDANTS_FIELD;
        const size_t field_len = sizeof(SELVA_DESCENDANTS_FIELD) - 1;

        SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node, field_str, field_len);
    }
}

static inline void publishChildrenUpdate(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node) {
    if (!isRdbLoading()) {
        const char *field_str = SELVA_CHILDREN_FIELD;
        const size_t field_len = sizeof(SELVA_CHILDREN_FIELD) - 1;

        SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node, field_str, field_len);
    }
}

static inline void publishParentsUpdate(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node) {
    if (!isRdbLoading()) {
        const char *field_str = SELVA_PARENTS_FIELD;
        const size_t field_len = sizeof(SELVA_PARENTS_FIELD) - 1;

        SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node, field_str, field_len);
    }
}

static int cross_insert_children(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        size_t n,
        const Selva_NodeId *nodes) {
    const int send_events = !isRdbLoading();
    int res = 0;

    if (n == 0) {
        return 0; /* No changes. */
    }

    if (unlikely(node->flags & SELVA_NODE_FLAGS_DETACHED)) {
        /* The subtree must be restored before adding nodes here. */
        SELVA_LOG(SELVA_LOGL_ERR, "FATAL Cannot add children to a detached node\n");
        return SELVA_HIERARCHY_ENOTSUP;
    }

    for (size_t i = 0; i < n; i++) {
        SelvaHierarchyNode *child;

        /* TODO Could we upsert here? */
        child = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
        if (!child) {
            int err;

            err = SelvaModify_SetHierarchy(hierarchy, nodes[i],
                    0, NULL,
                    0, NULL,
                    &child);
            if (err < 0) {
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to create a child \"%.*s\" for \"%.*s\": %s\n",
                        (int)SELVA_NODE_ID_SIZE, nodes[i],
                        (int)SELVA_NODE_ID_SIZE, node->id,
                        selva_strerror(err));
                continue;
            }
        }

        if (SVector_InsertFast(&node->children, child) == NULL) {
            /* The child node is no longer an orphan */
            if (SVector_Size(&child->parents) == 0) {
                rmHead(hierarchy, child);
            }

            (void)SVector_InsertFast(&child->parents, node);

#if 0
            fprintf(stderr, "%s:%d: Inserted %.*s.children <= %.*s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node->id,
                    (int)SELVA_NODE_ID_SIZE, child->id);
#endif

            /*
             * Inherit markers from the parent node to the new child.
             */
            SelvaSubscriptions_InheritParent(
                    hierarchy,
                    child->id, &child->metadata,
                    SVector_Size(&child->children),
                    node);

            /*
             * Inherit markers from the new child to the parent node.
             */
            SelvaSubscriptions_InheritChild(
                    hierarchy,
                    node->id, &node->metadata,
                    SVector_Size(&node->parents),
                    child);

            if (send_events) {
                /*
                 * Publish that the parents field was changed.
                 * Actual events are only sent if there are subscription markers
                 * set on this node.
                 */
                publishParentsUpdate(hierarchy, child);
                publishAncestorsUpdate(hierarchy, child);

                SelvaSubscriptions_DeferHierarchyEvents(hierarchy, child);
            }

            res++; /* Count actual insertions */
        }

        publishChildrenUpdate(hierarchy, node);
        publishDescendantsUpdate(hierarchy, node);
    }

    if (send_events) {
        SelvaSubscriptions_DeferHierarchyEvents(hierarchy, node);
    }

    /*
     * Publish that the children field was changed.
     */
    if (res > 0) {
        publishChildrenUpdate(hierarchy, node);
        publishDescendantsUpdate(hierarchy, node);
#if HIERARCHY_SORT_BY_DEPTH
        updateDepth(hierarchy, node);
#endif
    }

    return res;
}

static int cross_insert_parents(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        size_t n,
        const Selva_NodeId *nodes) {
    const int send_events = isRdbLoading();
    int res = 0;

    if (n == 0) {
        return 0; /* No changes. */
    }

    /* The node is no longer an orphan */
    if (SVector_Size(&node->parents) == 0) {
        rmHead(hierarchy, node);
    }

    for (size_t i = 0; i < n; i++) {
        SelvaHierarchyNode *parent;

        parent = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
        if (!parent) {
            int err;

            /* RFE no_root is not propagated */
            err = SelvaModify_SetHierarchy(hierarchy, nodes[i],
                    1, ((Selva_NodeId []){ ROOT_NODE_ID }),
                    0, NULL,
                    &parent);
            if (err < 0) {
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to create a parent \"%.*s\" for \"%.*s\": %s\n",
                        (int)SELVA_NODE_ID_SIZE, nodes[i],
                        (int)SELVA_NODE_ID_SIZE, node->id,
                        selva_strerror(err));
                continue;
            }
        }

        /* Do inserts only if the relationship doesn't exist already */
        if (SVector_InsertFast(&node->parents, parent) == NULL) {
            (void)SVector_InsertFast(&parent->children, node);

#if 0
            fprintf(stderr, "%s:%d: Inserted %.*s.parents <= %.*s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node->id,
                    (int)SELVA_NODE_ID_SIZE, parent->id);
#endif

            /*
             * Inherit subscription markers from the new parent to the node.
             */
            SelvaSubscriptions_InheritParent(
                    hierarchy,
                    node->id, &node->metadata,
                    SVector_Size(&node->children),
                    parent);

            /*
             * Inherit subscription markers from the node to the new parent.
             */
            SelvaSubscriptions_InheritChild(
                    hierarchy,
                    parent->id, &parent->metadata,
                    SVector_Size(&parent->parents),
                    node);

            if (send_events) {
                /*
                 * Publish that the children field was changed.
                 * Actual events are only sent if there are subscription markers
                 * set on this node.
                 */
                publishChildrenUpdate(hierarchy, parent);
                publishDescendantsUpdate(hierarchy, parent);

                SelvaSubscriptions_DeferHierarchyEvents(hierarchy, parent);
            }

            res++;
        }
    }

    if (send_events) {
        SelvaSubscriptions_DeferHierarchyEvents(hierarchy, node);
    }

    /*
     * Publish that the parents field was changed.
     */
    if (res > 0) {
        publishParentsUpdate(hierarchy, node);
        publishAncestorsUpdate(hierarchy, node);
#if HIERARCHY_SORT_BY_DEPTH
        updateDepth(hierarchy, node);
#endif
    }

    return res;
}

/*
 * @param pointers is set if nodes array contains pointers instead of Ids.
 */
static int crossRemove(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        enum SelvaHierarchyNode_Relationship rel,
        size_t n,
        const Selva_NodeId *nodes,
        int pointers) {
    SVECTOR_AUTOFREE(sub_markers);

    /*
     * Take a backup of the subscription markers so we can refresh them after
     * the operation.
     */
#ifndef PU_TEST_BUILD
    if (unlikely(!SVector_Clone(&sub_markers, &node->metadata.sub_markers.vec, NULL))) {
        return SELVA_HIERARCHY_ENOMEM;
    }
    SelvaSubscriptions_ClearAllMarkers(hierarchy, node);
#endif

    if (rel == RELATIONSHIP_CHILD) { /* no longer a child of adjacent */
        const size_t initialNodeParentsSize = SVector_Size(&node->parents);
        int pubParents = 0;

        for (size_t i = 0; i < n; i++) {
            SelvaHierarchyNode *parent;

            if (pointers) {
                memcpy(&parent, nodes[i], sizeof(SelvaHierarchyNode *));
            } else {
                parent = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
            }

            if (!parent) {
                /*
                 * The most Redis thing to do is probably to ignore any
                 * missing nodes.
                 */
                continue;
            }

            SVector_Remove(&parent->children, node);
            SVector_Remove(&node->parents, parent);

#if HIERARCHY_SORT_BY_DEPTH
            updateDepth(hierarchy, adjacent);
#endif
            publishChildrenUpdate(hierarchy, parent);
            pubParents = 1;
        }

        if (initialNodeParentsSize > 0 && SVector_Size(&node->parents) == 0) {
            /* node is an orphan now */
            mkHead(hierarchy, node);
        }

        if (pubParents) {
            publishParentsUpdate(hierarchy, node);
        }
    } else if (rel == RELATIONSHIP_PARENT) { /* no longer a parent of adjacent */
        int pubChildren = 0;

        for (size_t i = 0; i < n; i++) {
            SelvaHierarchyNode *child;

            if (pointers) {
                memcpy(&child, nodes[i], sizeof(SelvaHierarchyNode *));
            } else {
                child = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
            }

            if (!child) {
                /*
                 * The most Redis thing to do is probably to ignore any
                 * missing nodes.
                 */
                continue;
            }

            SVector_Remove(&child->parents, node);
            SVector_Remove(&node->children, child);

            if (SVector_Size(&child->parents) == 0) {
                /* child is an orphan now */
                mkHead(hierarchy, child);
            }

#if HIERARCHY_SORT_BY_DEPTH
            updateDepth(hierarchy, adjacent);
#endif
            publishParentsUpdate(hierarchy, child);
            pubChildren = 1;
        }

        if (pubChildren) {
            publishChildrenUpdate(hierarchy, node);
        }
    } else {
        return SELVA_HIERARCHY_ENOTSUP;
    }

#if HIERARCHY_SORT_BY_DEPTH
    updateDepth(hierarchy, node);
#endif
    SelvaSubscriptions_RefreshByMarker(hierarchy, &sub_markers);

    return 0;
}

/**
 * Remove all relationships rel of node.
 * @returns the number removed relationships. The nodes aren't necessary deleted.
 */
static int removeRelationships(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        enum SelvaHierarchyNode_Relationship rel) {
    SVector *vec_a;
    size_t offset_a;
    size_t offset_b;
    int nr_removed;
    SVECTOR_AUTOFREE(sub_markers);

    switch (rel) {
    case RELATIONSHIP_PARENT:
        /* Remove parent releationship to other nodes */
        offset_a = offsetof(SelvaHierarchyNode, children);
        offset_b = offsetof(SelvaHierarchyNode, parents);
        break;
    case RELATIONSHIP_CHILD:
        /* Remove child releationship to other nodes */
        offset_a = offsetof(SelvaHierarchyNode, parents);
        offset_b = offsetof(SelvaHierarchyNode, children);
        break;
    default:
        /* rel is invalid */
        assert(0);
        return 0;
    }

    vec_a = (SVector *)((char *)node + offset_a);
    nr_removed = SVector_Size(vec_a);
    if (nr_removed == 0) {
        return nr_removed;
    }

    /*
     * Backup the subscription markers so we can refresh them after the
     * operation.
     */
#ifndef PU_TEST_BUILD
    if (unlikely(!SVector_Clone(&sub_markers, &node->metadata.sub_markers.vec, NULL))) {
        SELVA_LOG(SELVA_LOGL_ERR, "Cloning markers failed\n");
        return SELVA_HIERARCHY_EINVAL;
    }
#endif

    SelvaSubscriptions_ClearAllMarkers(hierarchy, node);

    struct SVectorIterator it;
    SelvaHierarchyNode *adj;

    SVector_ForeachBegin(&it, vec_a);
    while ((adj = SVector_Foreach(&it))) {
        SVector *vec_b = (SVector *)((char *)adj + offset_b);

        SVector_Remove(vec_b, node);

        if (rel == RELATIONSHIP_PARENT && SVector_Size(vec_b) == 0) {
            /* This node is now orphan */
            mkHead(hierarchy, adj);
        }

#if HIERARCHY_SORT_BY_DEPTH
        updateDepth(hierarchy, it);
#endif
    }
    SVector_Clear(vec_a);

    SelvaSubscriptions_RefreshByMarker(hierarchy, &sub_markers);

    if (rel == RELATIONSHIP_CHILD) {
        mkHead(hierarchy, node);
    }

    return nr_removed;
}

int SelvaHierarchy_DelChildren(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node) {
    return removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);
}

int SelvaHierarchy_DelParents(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node) {
    return removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);
}

static inline SelvaHierarchyNode *index_new_node(SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    return RB_INSERT(hierarchy_index_tree, &hierarchy->index_head, node);
}

int SelvaModify_SetHierarchy(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children,
        struct SelvaHierarchyNode **node_out) {
    SelvaHierarchyNode *node;
    int isNewNode = 0;
    int err, res = 0;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        node = newNode(hierarchy, id);
        if (unlikely(!node)) {
            return SELVA_HIERARCHY_ENOMEM;
        }

        if (!isRdbLoading()) {
            new_node_events(hierarchy, node);
        }
        isNewNode = 1;
    }

    if (isNewNode) {
        if (unlikely(index_new_node(hierarchy, node))) {
            SelvaModify_DestroyNode(hierarchy, node);

            return SELVA_HIERARCHY_EEXIST;
        }

        if (nr_parents == 0) {
            /* This node is orphan */
            mkHead(hierarchy, node);
        }

        res++;
    } else {
        /*
         * Clear the existing node relationships.
         * Note that we can't really tell the called how many relationships were
         * removed because there is only one count we return.
         */
        (void)removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);
        (void)removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);
    }

    /*
     * Set relationship relative to other nodes
     * RFE if isNewNode == 0 then errors are not handled properly as
     * we don't know how to rollback.
     */
    err = cross_insert_parents(hierarchy, node, nr_parents, parents);
    if (err < 0) {
        if (isNewNode) {
            del_node(hierarchy, node);
        }
        return err;
    }
    res += err;

    /* Same for the children */
    err = cross_insert_children(hierarchy, node, nr_children, children);
    if (err < 0) {
        if (isNewNode) {
            del_node(hierarchy, node);
        }
        return err;
    }
    res += err;

    if (node_out) {
        *node_out = node;
    }

    return res;
}

/**
 * Remove adjacents not on the nodes list.
 */
static int remove_missing(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        size_t nr_nodes,
        const Selva_NodeId *nodes,
        enum SelvaHierarchyNode_Relationship rel) {
    SVECTOR_AUTOFREE(old_adjs);
    struct SVectorIterator it;
    SelvaHierarchyNode *adj;
    int res = 0;

    if (unlikely(!SVector_Clone(&old_adjs, rel == RELATIONSHIP_CHILD ? &node->parents : &node->children, NULL))) {
        SELVA_LOG(SELVA_LOGL_ERR, "SVector clone failed");
        return SELVA_HIERARCHY_ENOMEM;
    }

    SVector_ForeachBegin(&it, &old_adjs);
    while ((adj = SVector_Foreach(&it))) {
        int found = 0;

        for (size_t i = 0; i < nr_nodes; i++) {
            if (!memcmp(adj->id, nodes[i], SELVA_NODE_ID_SIZE)) {
                found = 1;
                break;
            }
        }

        if (!found) {
            Selva_NodeId arr[1];

#if 0
            fprintf(stderr, "%s:%d: Removing %.*s.%s.%.*s\n", __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node->id,
                    rel == RELATIONSHIP_CHILD ? SELVA_PARENTS_FIELD : SELVA_CHILDREN_FIELD,
                    (int)SELVA_NODE_ID_SIZE, adj->id);
#endif

            memcpy(arr, &adj, sizeof(SelvaHierarchyNode *));
            crossRemove(hierarchy, node, rel, 1, arr, 1);
            res++;
        }
    }

    return res;
}

int SelvaModify_SetHierarchyParents(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents) {
    SelvaHierarchyNode *node;
    int err, res = 0;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    if (nr_parents == 0) {
        /* Clear the existing node relationships. */
        return removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);
    }

    /*
     * Set relationship relative to other nodes.
     */
    err = cross_insert_parents(hierarchy, node, nr_parents, parents);
    if (err < 0) {
        return err;
    }
    res += err;

    /*
     * Remove parents that are not in the given list.
     */
    err = remove_missing(hierarchy, node, nr_parents, parents, RELATIONSHIP_CHILD);
    if (err < 0) {
        return err;
    }
    res += err;

    return res;
}

int SelvaModify_SetHierarchyChildren(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaHierarchyNode *node;
    int err, res = 0;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    if (nr_children == 0) {
        /* Clear the existing node relationships */
        return removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);
    }

    /*
     * Set relationship relative to other nodes.
     */
    err = cross_insert_children(hierarchy, node, nr_children, children);
    if (err < 0) {
        return err;
    }
    res += err;

    /*
     * Remove children that are not in the given list.
     */
    err = remove_missing(hierarchy, node, nr_children, children, RELATIONSHIP_PARENT);
    if (err < 0) {
        return err;
    }
    res += err;

    return res;
}

int SelvaHierarchy_UpsertNode(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        SelvaHierarchyNode **out) {
    SelvaHierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, id);
    SelvaHierarchyNode *prev_node;
    const int isLoading = isRdbLoading();

    if (node) {
        if (out) {
            *out = node;
        }

        return SELVA_HIERARCHY_EEXIST;
    }

     node = newNode(hierarchy, id);
     if (unlikely(!node)) {
         return SELVA_HIERARCHY_ENOMEM;
     }

     /*
      * No need to check if we have event registrations while loading the
      * database.
      */
     if (!isLoading) {
        new_node_events(hierarchy, node);
     }

     /*
      * All nodes must be indexed.
      */
     prev_node = index_new_node(hierarchy, node);
     if (prev_node) {
         /*
          * We are being extremely paranoid here as this shouldn't be possible.
          */
         SelvaModify_DestroyNode(hierarchy, node);

         if (out) {
             *out = prev_node;
         }
         return SELVA_HIERARCHY_EEXIST;
     }

     /*
      * This node is currently an orphan and it must be marked as such.
      */
     mkHead(hierarchy, node);

     if (out) {
         *out = node;
     }
     return 0;
}

int SelvaModify_AddHierarchyP(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    int err, res = 0;

    /*
     * Update relationship relative to other nodes
     * RFE if isNewNode == 0 then errors are not handled properly as
     * we don't know how to rollback.
     */
    err = cross_insert_parents(hierarchy, node, nr_parents, parents);
    if (err < 0) {
        return err;
    }
    res += err;

    /* Same for the children */
    err = cross_insert_children(hierarchy, node, nr_children, children);
    if (err < 0) {
        return err;
    }
    res += err;

    return res;
}

int SelvaModify_AddHierarchy(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaHierarchyNode *node;
    int isNewNode;
    int err;

    err = SelvaHierarchy_UpsertNode(hierarchy, id, &node);
    if (err == SELVA_HIERARCHY_EEXIST) {
        isNewNode = 0;
    } else if (err) {
        return err;
    } else {
        isNewNode = 1;
    }

    err = SelvaModify_AddHierarchyP(hierarchy, node, nr_parents, parents, nr_children, children);
    if (err < 0) {
        if (isNewNode) {
            del_node(hierarchy, node);
        }

        return err;
    }

    return err + isNewNode; /* Return the number of changes. */
}

int SelvaModify_DelHierarchy(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaHierarchyNode *node;
    int err1, err2;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    err1 = crossRemove(hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents, 0);
    err2 = crossRemove(hierarchy, node, RELATIONSHIP_PARENT, nr_children, children, 0);

    return err1 ? err1 : err2;
}

/**
 * Copy nodeIds from vec to dst array.
 * The dst array must be large enough.
 * @param dst is an array of Selva_NodeIds
 * @param vec is an SVector pointing to SelvaNodes.
 */
static void copy_nodeIds(Selva_NodeId *dst, const struct SVector *vec) {
    struct SVectorIterator it;
    const SelvaHierarchyNode *node;

    SVector_ForeachBegin(&it, vec);
    while ((node = SVector_Foreach(&it))) {
        memcpy(dst++, node->id, SELVA_NODE_ID_SIZE);
    }
}

static int subr_del_adj_relationship(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        const Selva_NodeId adj_node_id,
        enum SelvaHierarchyNode_Relationship dir,
        SelvaHierarchyNode **adj_node_out) {
    SelvaHierarchyNode *adj_node;
    Selva_NodeId arr[1];

    /*
     * Find the node.
     */
    adj_node = SelvaHierarchy_FindNode(hierarchy, adj_node_id);
    *adj_node_out = adj_node;
    if (!adj_node) {
        /* Node not found;
         * This is probably fine, as there might have been a circular link.
         */
        return SELVA_HIERARCHY_ENOENT;
    }

    /*
     * Note that we store a pointer in a Selva_NodeId array to save in
     * pointless RB_FIND() lookups.
     */
    memcpy(arr, &adj_node, sizeof(SelvaHierarchyNode *));
    return crossRemove(hierarchy, node, dir, 1, arr, 1);
}

/**
 * Delete a node and its children.
 * @param resp Set to alloc the caller to send deleted ids; Can be NULL if DEL_HIERARCHY_NODE_REPLY_IDS is not given.
 * @param flags controlling how the deletion is executed.
 * @param opt_arg is a pointer to an optional argument, depending on flags.
 */
static int SelvaModify_DelHierarchyNodeP(
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        enum SelvaModify_DelHierarchyNodeFlag flags,
        void *opt_arg) {
    size_t nr_ids;
    int nr_deleted = 0;

    assert(hierarchy);
    assert(node);

    if (flags & DEL_HIERARCHY_NODE_DETACH) {
        if (!opt_arg) {
            return SELVA_HIERARCHY_EINVAL;
        }

        /* Add to the detached nodes. */
        /* FIXME Implement node detaching */
#if 0
        SelvaHierarchyDetached_AddNode(hierarchy, node->id, opt_arg);
#endif
    } else if (node->flags & SELVA_NODE_FLAGS_DETACHED) {
        /*
         * This should only happen if we have failed to restore the
         * subtree.
         */
        return SELVA_HIERARCHY_ENOTSUP;
    }

    SelvaSubscriptions_ClearAllMarkers(hierarchy, node);

    /*
     * Delete links to parents.
     * This might seem like unnecessary as the parent links will be deleted
     * when then node is deleted. However, if there is a cycle back to this
     * node from its descendants then we'd loop back here and eventually
     * causing invalid/NULL pointers to appear.
     */
    nr_ids = SVector_Size(&node->parents);
    if (nr_ids > 0) {
        Selva_NodeId *ids;

        /* TODO maybe alloca is not good here */
        ids = alloca(nr_ids * SELVA_NODE_ID_SIZE);

        copy_nodeIds(ids, &node->parents);
        for (size_t i = 0; i < nr_ids; i++) {
            SelvaHierarchyNode *parent;
            int err;

            err = subr_del_adj_relationship(hierarchy, node, ids[i], RELATIONSHIP_CHILD, &parent);
            if (err == SELVA_HIERARCHY_ENOENT) {
                continue;
            } else if (err) {
                return err;
            }
        }
    }

    /*
     * Delete orphan children recursively.
     */
    nr_ids = SVector_Size(&node->children);
    if (nr_ids > 0) {
        Selva_NodeId *ids;

        ids = alloca(nr_ids * SELVA_NODE_ID_SIZE); /* TODO Perhaps replace with something else */

        copy_nodeIds(ids, &node->children);
        for (size_t i = 0; i < nr_ids; i++) {
            SelvaHierarchyNode *child;
            int err;

            err = subr_del_adj_relationship(hierarchy, node, ids[i], RELATIONSHIP_PARENT, &child);
            if (err == SELVA_HIERARCHY_ENOENT) {
                continue;
            } else if (err) {
                return err;
            }

            /*
             * Recursively delete the child and its children if its parents field is
             * empty and no edge fields are pointing to it.
             */
            if ((flags & DEL_HIERARCHY_NODE_FORCE) || (SVector_Size(&child->parents) == 0 && Edge_Refcount(child) == 0)) {
                err = SelvaModify_DelHierarchyNodeP(resp, hierarchy, child, flags, opt_arg);
                if (err < 0) {
                    return err;
                } else {
                    nr_deleted += err;
                }
            }
        }
    }

    if ((flags & DEL_HIERARCHY_NODE_REPLY_IDS) != 0) {
        assert(resp);
        selva_send_str(resp, node->id, Selva_NodeIdLen(node->id));
    }
    del_node(hierarchy, node);

    return nr_deleted + 1;
}

int SelvaModify_DelHierarchyNode(
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaModify_DelHierarchyNodeFlag flags) {
    SelvaHierarchyNode *node;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return SelvaModify_DelHierarchyNodeP(resp, hierarchy, node, flags, NULL);
}

static int SelvaHierarchyHeadCallback_Dummy(
        struct SelvaHierarchy *hierarchy __unused,
        SelvaHierarchyNode *node __unused,
        void *arg __unused) {
    return 0;
}

static int HierarchyNode_Callback_Dummy(
        struct SelvaHierarchy *hierarchy __unused,
        struct SelvaHierarchyNode *node __unused,
        void *arg __unused) {
    return 0;
}

static void SelvaHierarchyChildCallback_Dummy(
        struct SelvaHierarchy *hierarchy __unused,
        const struct SelvaHierarchyTraversalMetadata *metadata __unused,
        struct SelvaHierarchyNode *child __unused,
        void *arg __unused) {
}

/**
 * DFS from a given head node towards its descendants or ancestors.
 */
static int dfs(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *head,
        enum SelvaHierarchyNode_Relationship dir,
        const struct SelvaHierarchyCallback * restrict cb) {
    SelvaHierarchyHeadCallback head_cb = cb->head_cb ? cb->head_cb : &SelvaHierarchyHeadCallback_Dummy;
    SelvaHierarchyNodeCallback node_cb = cb->node_cb ? cb->node_cb : &HierarchyNode_Callback_Dummy;
    SelvaHierarchyChildCallback child_cb = cb->child_cb ? cb->child_cb : &SelvaHierarchyChildCallback_Dummy;
    size_t offset;
    struct SelvaHierarchyTraversalMetadata child_metadata;

    switch (dir) {
    case RELATIONSHIP_PARENT:
        child_metadata.origin_field_str = (const char *)SELVA_PARENTS_FIELD;
        child_metadata.origin_field_len = sizeof(SELVA_PARENTS_FIELD) - 1;
        offset = offsetof(SelvaHierarchyNode, parents);
        break;
    case RELATIONSHIP_CHILD:
        child_metadata.origin_field_str = (const char *)SELVA_CHILDREN_FIELD;
        child_metadata.origin_field_len = sizeof(SELVA_CHILDREN_FIELD) - 1;
        offset = offsetof(SelvaHierarchyNode, children);
        break;
    default:
        return SELVA_HIERARCHY_ENOTSUP;
    }

    SVECTOR_AUTOFREE(stack);
    SVector_Init(&stack, selva_glob_config.hierarchy_expected_resp_len, NULL);

    int err = 0;
    struct trx trx_cur;
    if (Trx_Begin(&hierarchy->trx_state, &trx_cur)) {
        return SELVA_HIERARCHY_ETRMAX;
    }

    SVector_Insert(&stack, head);
    if (head_cb(hierarchy, head, cb->head_arg)) {
        err = 0;
        goto out;
    }

    while (SVector_Size(&stack) > 0) {
        SelvaHierarchyNode *node;

        node = SVector_Pop(&stack);
        if (Trx_Visit(&trx_cur, &node->trx_label)) {
            if (node_cb(hierarchy, node, cb->node_arg)) {
                err = 0;
                goto out;
            }

            /* Add parents/children of this node to the stack of unvisited nodes */
            struct SVectorIterator it;
            SelvaHierarchyNode *adj;
            const SVector *vec = (SVector *)((char *)node + offset);

            SVector_ForeachBegin(&it, vec);
            while ((adj = SVector_Foreach(&it))) {
                if (adj->flags & SELVA_NODE_FLAGS_DETACHED) {
                    err = restore_subtree(hierarchy, adj->id);
                    if (err) {
                        /*
                         * The error is already logged,
                         * we just try to bail from here.
                         */
                        goto out;
                    }
                }

                child_metadata.origin_node = node;
                child_cb(hierarchy, &child_metadata, adj, cb->child_arg);

                /* Add to the stack of unvisited nodes */
                SVector_Insert(&stack, adj);
            }
        }
    }

out:
    Trx_End(&hierarchy->trx_state, &trx_cur);
    return err;
}

/**
 * Traverse through all nodes of the hierarchy from heads to leaves.
 */
static int full_dfs(
        struct SelvaHierarchy *hierarchy,
        const struct SelvaHierarchyCallback * restrict cb) {
    SelvaHierarchyHeadCallback head_cb = cb->head_cb ? cb->head_cb : &SelvaHierarchyHeadCallback_Dummy;
    SelvaHierarchyNodeCallback node_cb = cb->node_cb ? cb->node_cb : &HierarchyNode_Callback_Dummy;
    SelvaHierarchyChildCallback child_cb = cb->child_cb ? cb->child_cb : &SelvaHierarchyChildCallback_Dummy;
    const int enable_restore = !(cb->flags & SELVA_HIERARCHY_CALLBACK_FLAGS_INHIBIT_RESTORE);
    SelvaHierarchyNode *head;
    SVECTOR_AUTOFREE(stack);

    SVector_Init(&stack, selva_glob_config.hierarchy_expected_resp_len, NULL);

    struct trx trx_cur;
    if (Trx_Begin(&hierarchy->trx_state, &trx_cur)) {
        return SELVA_HIERARCHY_ETRMAX;
    }

    int err = 0;
    struct SVectorIterator it;
    struct SelvaHierarchyTraversalMetadata child_metadata = {
        .origin_field_str = (const char *)"children",
        .origin_field_len = 8,
    };

    /**
     * Set if we should track inactive nodes for auto compression.
     */
    const int enAutoCompression = selva_glob_config.hierarchy_auto_compress_period_ms > 0 && isRdbSaving;
    const long long old_age_threshold = selva_glob_config.hierarchy_auto_compress_old_age_lim;

    SVector_ForeachBegin(&it, &hierarchy->heads);
    while ((head = SVector_Foreach(&it))) {
        SVector_Insert(&stack, head);

        if ((head->flags & SELVA_NODE_FLAGS_DETACHED) && enable_restore) {
            err = restore_subtree(hierarchy, head->id);
            if (err) {
                /*
                 * The error is already logged,
                 * we just try to bail from here.
                 */
                goto out;
            }
        }

        if (head_cb(hierarchy, head, cb->head_arg)) {
            err = 0;
            goto out;
        }

        /**
         * This variable tracks a contiguous (DFS) path that hasn't been
         * traversed for some time. It starts tracking when the first old node
         * is found and keeps its value unless a subsequent node has been
         * touched recently.
         * The candidate is saved and the variable is reset to NULL once a leaf
         * is reached, and the tracking can start again from the top.
         */
        struct SelvaHierarchyNode *compressionCandidate = NULL;

        while (SVector_Size(&stack) > 0) {
            SelvaHierarchyNode *node = SVector_Pop(&stack);

            /*
             * Note that the RDB save child process won't touch the trxids in
             * the parent process (separate address space), therefore old nodes
             * will generally stay old if they are otherwise untouched.
             */
            if (enAutoCompression && !compressionCandidate &&
                memcmp(node->id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE)) {
                if (Trx_LabelAge(&hierarchy->trx_state, &node->trx_label) >= old_age_threshold &&
                    !(node->flags & SELVA_NODE_FLAGS_DETACHED)) {
                    compressionCandidate = node;
                }
            }
            if (compressionCandidate && Trx_LabelAge(&hierarchy->trx_state, &node->trx_label) < old_age_threshold) {
                compressionCandidate = NULL;
            }

            if (Trx_Visit(&trx_cur, &node->trx_label)) {
                struct SVectorIterator it2;
                SelvaHierarchyNode *adj;

                if (node_cb(hierarchy, node, cb->node_arg)) {
                    err = 0;
                    goto out;
                }
                if (node->flags & SELVA_NODE_FLAGS_DETACHED) {
                    /*
                     * Can't traverse a detached node any further.
                     * This flag can be set only if we inhibit restoring
                     * subtrees with SELVA_HIERARCHY_CALLBACK_FLAGS_INHIBIT_RESTORE.
                     */
                    continue;
                }

                /*
                 * Reset the compressionCandidate tracking and save the current candidate.
                 * No need to test enAutoCompression as compressionCandidate
                 * would be NULL on false case.
                 * FIXME Fix inact nodes
                 */
#if 0
                if (compressionCandidate && SVector_Size(&node->children) == 0) {
                    SelvaHierarchy_AddInactiveNodeId(hierarchy, compressionCandidate->id);
                    compressionCandidate = NULL;
                }
#endif

                SVector_ForeachBegin(&it2, &node->children);
                while ((adj = SVector_Foreach(&it2))) {
                    if ((adj->flags & SELVA_NODE_FLAGS_DETACHED) && enable_restore) {
                        err = restore_subtree(hierarchy, adj->id);
                        if (err) {
                            goto out;
                        }
                    }

                    child_metadata.origin_node = node; /* parent */
                    child_cb(hierarchy, &child_metadata, adj, cb->child_arg);

                    /* Add to the stack of unvisited nodes */
                    SVector_Insert(&stack, adj);
                }
            }
        }
    }

out:
    Trx_End(&hierarchy->trx_state, &trx_cur);
    return err;
}

#define BFS_TRAVERSE(ctx, hierarchy, head, cb) \
    SelvaHierarchyHeadCallback head_cb = (cb)->head_cb ? (cb)->head_cb : SelvaHierarchyHeadCallback_Dummy; \
    SelvaHierarchyNodeCallback node_cb = (cb)->node_cb ? (cb)->node_cb : HierarchyNode_Callback_Dummy; \
    SelvaHierarchyChildCallback child_cb = (cb)->child_cb ? (cb)->child_cb : SelvaHierarchyChildCallback_Dummy; \
    \
    SVECTOR_AUTOFREE(_bfs_q); \
    SVector_Init(&_bfs_q, selva_glob_config.hierarchy_expected_resp_len, NULL); \
    \
    struct trx trx_cur; \
    if (Trx_Begin(&(hierarchy)->trx_state, &trx_cur)) { \
        return SELVA_HIERARCHY_ETRMAX; \
    } \
    \
    Trx_Visit(&trx_cur, &(head)->trx_label); \
    SVector_Insert(&_bfs_q, (head)); \
    if (head_cb((hierarchy), (head), (cb)->head_arg)) { Trx_End(&(hierarchy)->trx_state, &trx_cur); return 0; } \
    while (SVector_Size(&_bfs_q) > 0) { \
        SelvaHierarchyNode *node = SVector_Shift(&_bfs_q);

#define BFS_VISIT_NODE(ctx, hierarchy) \
        /* Note that Trx_Visit() has been already called for this node. */ \
        if (node_cb((hierarchy), node, cb->node_arg)) { \
            Trx_End(&(hierarchy)->trx_state, &trx_cur); \
            return 0; \
        }

#define BFS_VISIT_ADJACENT(ctx, hierarchy, _origin_field_str, _origin_field_len, adj_node) do { \
        if (Trx_Visit(&trx_cur, &(adj_node)->trx_label)) { \
            if ((adj_node)->flags & SELVA_NODE_FLAGS_DETACHED) { \
                int subtree_err = restore_subtree((hierarchy), (adj_node)->id); \
                if (subtree_err) { \
                    Trx_End(&(hierarchy)->trx_state, &trx_cur); \
                    return subtree_err; \
                } \
            } \
            const struct SelvaHierarchyTraversalMetadata _cb_metadata = { \
                .origin_field_str = (_origin_field_str), \
                .origin_field_len = (_origin_field_len), \
                .origin_node = node, \
            }; \
            child_cb((hierarchy), &_cb_metadata, (adj_node), cb->child_arg); \
            SVector_Insert(&_bfs_q, (adj_node)); \
        } \
    } while (0)

#define BFS_VISIT_ADJACENTS(ctx, hierarchy, origin_field_str, origin_field_len, adj_vec) do { \
        struct SVectorIterator _bfs_visit_it; \
        \
        SVector_ForeachBegin(&_bfs_visit_it, (adj_vec)); \
        SelvaHierarchyNode *_adj; \
        while ((_adj = SVector_Foreach(&_bfs_visit_it))) { \
            BFS_VISIT_ADJACENT((ctx), (hierarchy), (origin_field_str), (origin_field_len), _adj); \
        } \
    } while (0)

#define BFS_TRAVERSE_END(hierarchy) \
    } \
    Trx_End(&(hierarchy)->trx_state, &trx_cur)

/**
 * Get the next vector to be traversed.
 * @param node is the current node the traversal is visiting.
 * @param field is the next field to be traversed.
 * @param[out] field_type returns the type of the field being traversed.
 */
static SVector *get_adj_vec(SelvaHierarchyNode *node, const char *field_str, size_t field_len, enum SelvaTraversal *field_type) {
#define IS_FIELD(name) \
    (field_len == (sizeof(name) - 1) && !memcmp(name, field_str, sizeof(name) - 1))

    if (IS_FIELD(SELVA_CHILDREN_FIELD)) {
        *field_type = SELVA_HIERARCHY_TRAVERSAL_CHILDREN;
        return &node->children;
    } else if (IS_FIELD(SELVA_PARENTS_FIELD)) {
        *field_type = SELVA_HIERARCHY_TRAVERSAL_PARENTS;
        return &node->parents;
    } else {
        /* Try EdgeField */
        struct EdgeField *edge_field;

        edge_field = Edge_GetField(node, field_str, field_len);
        if (edge_field) {
            *field_type = SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD;
            return &edge_field->arcs;
        }
    }

    *field_type = SELVA_HIERARCHY_TRAVERSAL_NONE;
    return NULL;
#undef IS_FIELD
}

/**
 * Execute an edge filter for the node.
 * @param ctx is a pointer to the Redis context.
 * @param edge_filter_ctx is a context for the filter.
 * @param edge_filter is a pointer to the compiled edge filter.
 * @param adj_vec is a pointer to the arcs vector field of an EdgeField structure.
 * @param node is a pointer to the node the edge is pointing to.
 */
__attribute__((nonnull (5))) static int exec_edge_filter(
        struct SelvaHierarchy *hierarchy,
        struct rpn_ctx *edge_filter_ctx,
        const struct rpn_expression *edge_filter,
        const SVector *adj_vec,
        struct SelvaHierarchyNode *node) {
    struct EdgeField *edge_field = containerof(adj_vec, struct EdgeField, arcs);
    STATIC_SELVA_OBJECT(tmp_obj);
    struct SelvaObject *edge_metadata;
    int err;
    enum rpn_error rpn_err;
    int res;

    err = Edge_GetFieldEdgeMetadata(edge_field, node->id, 0, &edge_metadata);
    if (err == SELVA_HIERARCHY_ENOENT || err == SELVA_ENOENT) {
        /* Execute the filter with an empty object. */
        edge_metadata = SelvaObject_Init(tmp_obj);
    } else if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to get edge metadata %.*s -> %.*s\n",
                  (int)SELVA_NODE_ID_SIZE, edge_field->src_node_id,
                  (int)SELVA_NODE_ID_SIZE, node->id);
        return 0;
    }

    rpn_set_reg(edge_filter_ctx, 0, node->id, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
    rpn_set_hierarchy_node(edge_filter_ctx, hierarchy, node);
    rpn_set_obj(edge_filter_ctx, edge_metadata);
    rpn_err = rpn_bool(edge_filter_ctx, edge_filter, &res);

    return (!rpn_err && res) ? 1 : 0;
}

/**
 * BFS from a given head node towards its descendants or ancestors.
 */
static __hot int bfs(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *head,
        enum SelvaHierarchyNode_Relationship dir,
        const struct SelvaHierarchyCallback * restrict cb) {
    const char *origin_field_str;
    size_t origin_field_len;
    size_t offset;

    switch (dir) {
    case RELATIONSHIP_PARENT:
        origin_field_str = (const char *)SELVA_PARENTS_FIELD;
        origin_field_len = sizeof(SELVA_PARENTS_FIELD) - 1;
        offset = offsetof(SelvaHierarchyNode, parents);
        break;
    case RELATIONSHIP_CHILD:
        origin_field_str = (const char *)SELVA_CHILDREN_FIELD;
        origin_field_len = sizeof(SELVA_CHILDREN_FIELD) - 1;
        offset = offsetof(SelvaHierarchyNode, children);
        break;
    default:
        return SELVA_HIERARCHY_ENOTSUP;
    }

    BFS_TRAVERSE(ctx, hierarchy, head, cb) {
        const SVector *adj_vec = (SVector *)((char *)node + offset);

        BFS_VISIT_NODE(ctx, hierarchy);
        BFS_VISIT_ADJACENTS(ctx, hierarchy, origin_field_str, origin_field_len, adj_vec);
    } BFS_TRAVERSE_END(hierarchy);

    return 0;
}

static int bfs_edge(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *head,
        const char *field_name_str,
        size_t field_name_len,
        const struct SelvaHierarchyCallback * restrict cb) {
    BFS_TRAVERSE(ctx, hierarchy, head, cb) {
        const struct EdgeField *edge_field;

        BFS_VISIT_NODE(ctx, hierarchy);

        edge_field = Edge_GetField(node, field_name_str, field_name_len);
        if (!edge_field) {
#if 0
            fprintf(stderr, "Edge field %.*s not found in %.*s\n",
                    (int)field_name_len, field_name_str,
                    (int)SELVA_NODE_ID_SIZE, node->id);
#endif
            /* EdgeField not found! */
            continue;
        }

        BFS_VISIT_ADJACENTS(ctx, hierarchy, field_name_str, field_name_len, &edge_field->arcs);
    } BFS_TRAVERSE_END(hierarchy);

    return 0;
}

static int bfs_expression(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *head,
        struct rpn_ctx *rpn_ctx,
        const struct rpn_expression *rpn_expr,
        struct rpn_ctx *edge_filter_ctx,
        const struct rpn_expression *edge_filter,
        const struct SelvaHierarchyCallback * restrict cb) {
    BFS_TRAVERSE(redis_ctx, hierarchy, head, cb) {
        enum rpn_error rpn_err;
        struct SelvaSet fields;
        struct SelvaSetElement *field_el;

        SelvaSet_Init(&fields, SELVA_SET_TYPE_STRING);

        rpn_set_reg(rpn_ctx, 0, node->id, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
        rpn_set_hierarchy_node(rpn_ctx, hierarchy, node);
        rpn_set_obj(rpn_ctx, SelvaHierarchy_GetNodeObject(node));
        rpn_err = rpn_selvaset(rpn_ctx, rpn_expr, &fields);
        if (rpn_err) {
            SELVA_LOG(SELVA_LOGL_ERR, "RPN field selector expression failed for %.*s: %s\n",
                      (int)SELVA_NODE_ID_SIZE, node->id,
                      rpn_str_error[rpn_err]);
            continue;
        }

        BFS_VISIT_NODE(redis_ctx, hierarchy);

        SELVA_SET_STRING_FOREACH(field_el, &fields) {
            size_t field_len;
            const char *field_str = selva_string_to_str(field_el->value_string, &field_len);
            enum SelvaTraversal field_type;
            const SVector *adj_vec;
            struct SVectorIterator it;
            SelvaHierarchyNode *adj;

            /* Get an SVector for the field. */
            adj_vec = get_adj_vec(node, field_str, field_len, &field_type);
            if (!adj_vec) {
                continue;
            }

            /* Visit each node in this field. */
            SVector_ForeachBegin(&it, adj_vec);
            while ((adj = SVector_Foreach(&it))) {
                /*
                 * If the field is an edge field we can filter edges by the
                 * edge metadata.
                 */
                if (field_type == SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD && edge_filter &&
                    !Trx_HasVisited(&trx_cur, &adj->trx_label) && /* skip if already visited. */
                    !exec_edge_filter(hierarchy, edge_filter_ctx, edge_filter, adj_vec, adj)) {
                    continue;
                }

                BFS_VISIT_ADJACENT(redis_ctx, hierarchy, field_str, field_len, adj);
            }
        }

        SelvaSet_Destroy(&fields);
    } BFS_TRAVERSE_END(hierarchy);

    return 0;
}

static void traverse_adjacents(
        struct SelvaHierarchy *hierarchy,
        const SVector *adj_vec,
        const struct SelvaHierarchyCallback *cb) {
    struct SVectorIterator it;

    if (cb->node_cb) {
        SelvaHierarchyNode *node;

        SVector_ForeachBegin(&it, adj_vec);
        while ((node = SVector_Foreach(&it))) {
            Trx_Sync(&hierarchy->trx_state, &node->trx_label);

            /* RFE Should we also call child_cb? */
            if (cb->node_cb(hierarchy, node, cb->node_arg)) {
                break;
            }
        }
    }
}

static void traverse_edge_field(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *head,
        const char *ref_field_str,
        size_t ref_field_len,
        const struct SelvaHierarchyCallback *cb) {
    const struct EdgeField *edge_field;

    if (cb->head_cb && cb->head_cb(hierarchy, head, cb->head_arg)) {
        return;
    }

    if (cb->node_cb) {
        edge_field = Edge_GetField(head, ref_field_str, ref_field_len);
        if (edge_field) {
            traverse_adjacents(hierarchy, &edge_field->arcs, cb);
        }
    }
}

static int traverse_ref(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *head,
        const char *ref_field_str,
        size_t ref_field_len,
        const struct SelvaHierarchyCallback *cb) {
    struct SelvaObject *head_obj = GET_NODE_OBJ(head);
    struct SelvaSet *ref_set;

    if (cb->head_cb && cb->head_cb(hierarchy, head, cb->head_arg)) {
        return 0;
    }

    ref_set = SelvaObject_GetSetStr(head_obj, ref_field_str, ref_field_len);
    if (!ref_set) {
        return SELVA_HIERARCHY_ENOENT;
    }
    if (ref_set->type != SELVA_SET_TYPE_STRING) {
        return SELVA_EINTYPE;
    }

    struct SelvaSetElement *el;
    SELVA_SET_STRING_FOREACH(el, ref_set) {
        Selva_NodeId nodeId;
        SelvaHierarchyNode *node;

        selva_string2node_id(nodeId, el->value_string);
        node = SelvaHierarchy_FindNode(hierarchy, nodeId);
        if (node) {
            if (cb->node_cb(hierarchy, node, cb->node_arg)) {
                return 0;
            }
        }
    }

    return 0;
}

static int traverse_bfs_edge_field(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        const char *field_name_str,
        size_t field_name_len,
        const struct SelvaHierarchyCallback *cb) {
    struct SelvaHierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return bfs_edge(hierarchy, head, field_name_str, field_name_len, cb);
}

void SelvaHierarchy_TraverseChildren(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb) {
    if (cb->head_cb && cb->head_cb(hierarchy, node, cb->head_arg)) {
        return;
    }

    traverse_adjacents(hierarchy, &node->children, cb);
}

void SelvaHierarchy_TraverseParents(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb) {
    if (cb->head_cb && cb->head_cb(hierarchy, node, cb->head_arg)) {
        return;
    }

    traverse_adjacents(hierarchy, &node->parents, cb);
}

int SelvaHierarchy_TraverseBFSAncestors(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb) {
    return bfs(hierarchy, node, RELATIONSHIP_PARENT, cb);
}

int SelvaHierarchy_TraverseBFSDescendants(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const struct SelvaHierarchyCallback *cb) {
    return bfs(hierarchy, node, RELATIONSHIP_CHILD, cb);
}

int SelvaHierarchy_Traverse(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaTraversal dir,
        const struct SelvaHierarchyCallback *cb) {
    SelvaHierarchyNode *head;
    int err = 0;

    if (dir == SELVA_HIERARCHY_TRAVERSAL_NONE) {
        return SELVA_HIERARCHY_EINVAL;
    }

    if (dir != SELVA_HIERARCHY_TRAVERSAL_DFS_FULL) {
        head = SelvaHierarchy_FindNode(hierarchy, id);
        if (!head) {
            return SELVA_HIERARCHY_ENOENT;
        }

        Trx_Sync(&hierarchy->trx_state, &head->trx_label);
    }

    switch (dir) {
    case SELVA_HIERARCHY_TRAVERSAL_NODE:
        cb->node_cb(hierarchy, head, cb->node_arg);
        break;
    case SELVA_HIERARCHY_TRAVERSAL_CHILDREN:
        SelvaHierarchy_TraverseChildren(hierarchy, head, cb);
        break;
    case SELVA_HIERARCHY_TRAVERSAL_PARENTS:
        SelvaHierarchy_TraverseParents(hierarchy, head, cb);
        break;
    case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
        err = bfs(hierarchy, head, RELATIONSHIP_PARENT, cb);
        break;
    case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
        err = bfs(hierarchy, head, RELATIONSHIP_CHILD, cb);
        break;
    case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
        err = dfs(hierarchy, head, RELATIONSHIP_PARENT, cb);
        break;
     case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
        err = dfs(hierarchy, head, RELATIONSHIP_CHILD, cb);
        break;
     case SELVA_HIERARCHY_TRAVERSAL_DFS_FULL:
        err = full_dfs(hierarchy, cb);
        break;
     default:
        /* Should probably use some other traversal function. */
        SELVA_LOG(SELVA_LOGL_ERR, "Invalid or unsupported traversal requested (%d)\n", (int)dir);
        err = SELVA_HIERARCHY_ENOTSUP;
    }

    return err;
}

int SelvaHierarchy_TraverseField(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaTraversal dir,
        const char *field_name_str,
        size_t field_name_len,
        const struct SelvaHierarchyCallback *cb) {
    SelvaHierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    Trx_Sync(&hierarchy->trx_state, &head->trx_label);

    switch (dir) {
    case SELVA_HIERARCHY_TRAVERSAL_REF:
        return traverse_ref(hierarchy, head, field_name_str, field_name_len, cb);
    case SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD:
        traverse_edge_field(hierarchy, head, field_name_str, field_name_len, cb);
        return 0;
    case SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD:
        return traverse_bfs_edge_field(hierarchy, id, field_name_str, field_name_len, cb);
     default:
        /* Should probably use some other traversal function. */
        SELVA_LOG(SELVA_LOGL_ERR, "Invalid traversal requested (%d)\n", (int)dir);
        return SELVA_HIERARCHY_ENOTSUP;
    }
}

int SelvaHierarchy_TraverseExpression(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        struct rpn_ctx *rpn_ctx,
        const struct rpn_expression *rpn_expr,
        struct rpn_ctx *edge_filter_ctx,
        const struct rpn_expression *edge_filter,
        const struct SelvaHierarchyCallback *cb) {
    SelvaHierarchyNode *head;
    struct trx trx_cur;
    enum rpn_error rpn_err;
    struct SelvaSet fields;
    struct SelvaSetElement *field_el;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    if (Trx_Begin(&hierarchy->trx_state, &trx_cur)) {
        return SELVA_HIERARCHY_ETRMAX;
    }

    SelvaSet_Init(&fields, SELVA_SET_TYPE_STRING);

    rpn_set_reg(rpn_ctx, 0, head->id, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
    rpn_set_hierarchy_node(rpn_ctx, hierarchy, head);
    rpn_set_obj(rpn_ctx, SelvaHierarchy_GetNodeObject(head));
    rpn_err = rpn_selvaset(rpn_ctx, rpn_expr, &fields);
    if (rpn_err) {
        Trx_End(&hierarchy->trx_state, &trx_cur);
        SELVA_LOG(SELVA_LOGL_ERR, "RPN field selector expression failed for %.*s: %s\n",
                  (int)SELVA_NODE_ID_SIZE, head->id,
                  rpn_str_error[rpn_err]);
        return SELVA_HIERARCHY_EINVAL;
    }

    /* For each field in the set. */
    SELVA_SET_STRING_FOREACH(field_el, &fields) {
        size_t field_len;
        const char *field_str = selva_string_to_str(field_el->value_string, &field_len);
        enum SelvaTraversal field_type;
        const SVector *adj_vec;
        struct SVectorIterator it;
        SelvaHierarchyNode *adj;

        /* Get an SVector for the field. */
        adj_vec = get_adj_vec(head, field_str, field_len, &field_type);
        if (!adj_vec) {
            continue;
        }

        /* Visit each node in this field. */
        SVector_ForeachBegin(&it, adj_vec);
        while ((adj = SVector_Foreach(&it))) {
            /*
             * If the field is an edge field we can filter edges by the
             * edge metadata.
             */
            if (field_type == SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD && edge_filter &&
                !Trx_HasVisited(&trx_cur, &adj->trx_label) && /* skip if already visited. */
                !exec_edge_filter(hierarchy, edge_filter_ctx, edge_filter, adj_vec, adj)) {
                continue;
            }

            if (Trx_Visit(&trx_cur, &adj->trx_label)) {
                if (cb->node_cb(hierarchy, adj, cb->node_arg)) {
                    Trx_End(&hierarchy->trx_state, &trx_cur);
                    return 0;
                }
            }
        }
    }

    Trx_End(&hierarchy->trx_state, &trx_cur);
    return 0;
}

int SelvaHierarchy_TraverseExpressionBfs(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        struct rpn_ctx *rpn_ctx,
        const struct rpn_expression *rpn_expr,
        struct rpn_ctx *edge_filter_ctx,
        const struct rpn_expression *edge_filter,
        const struct SelvaHierarchyCallback *cb) {
    SelvaHierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return bfs_expression(hierarchy, head, rpn_ctx, rpn_expr, edge_filter_ctx, edge_filter, cb);
}

int SelvaHierarchy_TraverseArray(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        const char *field_str,
        size_t field_len,
        const struct SelvaObjectArrayForeachCallback *cb) {
    struct SelvaHierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    Trx_Sync(&hierarchy->trx_state, &head->trx_label);

    return SelvaObject_ArrayForeach(GET_NODE_OBJ(head), field_str, field_len, cb);
}

int SelvaHierarchy_TraverseSet(
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        const char *field_str,
        size_t field_len,
        const struct SelvaObjectSetForeachCallback *cb) {
    struct SelvaHierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    Trx_Sync(&hierarchy->trx_state, &head->trx_label);

    return SelvaObject_SetForeach(GET_NODE_OBJ(head), field_str, field_len, cb);
}

int SelvaHierarchy_IsNonEmptyField(const struct SelvaHierarchyNode *node, const char *field_str, size_t field_len) {
#define IS_FIELD(name) \
    (field_len == (sizeof(name) - 1) && !memcmp(name, field_str, sizeof(name) - 1))

    if (IS_FIELD(SELVA_PARENTS_FIELD) ||
        IS_FIELD(SELVA_ANCESTORS_FIELD)) {
        return SVector_Size(&node->parents) > 0;
    } else if (IS_FIELD(SELVA_CHILDREN_FIELD) ||
               IS_FIELD(SELVA_DESCENDANTS_FIELD)) {
        return SVector_Size(&node->children) > 0;
    } else if (field_len > 0) {
        /*
         * Check if field is a custom edge field name.
         */
        const struct EdgeField *edge_field;

        edge_field = Edge_GetField(node, field_str, field_len);
        if (!edge_field) {
            return 0;
        }

        return SVector_Size(&edge_field->arcs);
    }

    return 0;
#undef IS_FIELD
}

/**
 * DO NOT CALL DIRECTLY. USE verifyDetachableSubtree().
 */
static int verifyDetachableSubtreeNodeCb(
        struct SelvaHierarchy *hierarchy __unused,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct verifyDetachableSubtree *data = (struct verifyDetachableSubtree *)arg;
    struct SVectorIterator it;
    const SelvaHierarchyNode *parent;

    /*
     * If edges from other nodes are pointing to this node, we want to
     * verify it later that all those edges are within the subtree.
     */
    if (Edge_Usage(node) & 2) {
        struct SelvaObject *origins = node->metadata.edge_fields.origins;
        SelvaObject_Iterator *it;
        const char *origin;

        it = SelvaObject_ForeachBegin(origins);
        while ((origin = SelvaObject_ForeachKey(origins, &it))) {
            Selva_NodeId origin_id;

            Selva_NodeIdCpy(origin_id, origin);
            SelvaSet_Add(&data->edge_origin_node_ids, origin_id);
        }
    }

    /*
     * Check that there are no active subscription markers on the node.
     * Subs starting from root can be ignored.
     */
    if (SelvaSubscriptions_hasActiveMarkers(&node->metadata)) {
        data->err = "markers";
        return 1;
    }

    /*
     * A subtree is allowed be a acyclic but `node` must be its true parent,
     * i.e. the whole subtree has only a single root node that is `node`.
     */
    SVector_ForeachBegin(&it, &node->parents);
    if (node != data->head) {
        while ((parent = SVector_Foreach(&it))) {
            if (!Trx_HasVisited(&data->trx_cur, &parent->trx_label)) {
                data->err = "not_tree";
                return 1; /* not a proper subtree. */
            }
        }
    }

    Trx_Visit(&data->trx_cur, &node->trx_label);

    return 0;
}

/**
 * Verify that the children of node can be safely detached.
 * Detachable subtree is subnetwork that the descendants of node can be safely
 * removed from the hierarchy, serialized and freed from memory.
 * This function checks that the children of node form a proper subtree that
 * and there are no active subscription markers or other live dependencies on
 * any of the nodes.
 * @return 0 is returned if the subtree is detachable;
 *         Otherwise a SelvaError is returned.
 */
static int verifyDetachableSubtree(struct SelvaHierarchy *hierarchy, struct SelvaHierarchyNode *node) {
    struct trx_state * restrict trx_state = &hierarchy->trx_state;
    struct verifyDetachableSubtree data = {
        .err = NULL,
        .head = node,
    };
    const struct SelvaHierarchyCallback cb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = verifyDetachableSubtreeNodeCb,
        .node_arg = &data,
        .child_cb = NULL,
        .child_arg = NULL,
    };
    int err;

    if (!Trx_Fin(trx_state)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Cannot compress a subtree while another transaction is being executed\n");
        return SELVA_HIERARCHY_ETRMAX;
    }

    if (Trx_Begin(trx_state, &data.trx_cur)) {
        return SELVA_HIERARCHY_ETRMAX;
    }
    SelvaSet_Init(&data.edge_origin_node_ids, SELVA_SET_TYPE_NODEID);

    err = bfs(hierarchy, node, RELATIONSHIP_CHILD, &cb);
    if (!err && data.err) {
        err = SELVA_HIERARCHY_ENOTSUP;
    }

    /*
     * Verify that all edge sources were visited by the traversal.
     * TODO It would be good to allow referenced nodes that are not
     * children in the subtree but still contained within the subgraph.
     */
    if (!err && SelvaSet_Size(&data.edge_origin_node_ids) > 0) {
        struct SelvaSetElement *el;

        SELVA_SET_NODEID_FOREACH(el, &data.edge_origin_node_ids) {
            SelvaHierarchyNode *node;

            node = find_node_index(hierarchy, el->value_nodeId);
            if (!node ||
                node->trx_label.id != data.trx_cur.id ||
                ((data.trx_cur.cl << 1) & node->trx_label.cl) == 0) {
                err = SELVA_HIERARCHY_ENOTSUP;
            }
        }
    }

    SelvaSet_Destroy(&data.edge_origin_node_ids);
    Trx_End(trx_state, &data.trx_cur);

    return err;
}

/**
 * Compress a subtree using DFS starting from node.
 * @returns The compressed tree is returned as a compressed_rms structure.
 */
static struct compressed_rms *compress_subtree(SelvaHierarchy *hierarchy, struct SelvaHierarchyNode *node, double *cratio) {
    /* FIXME Fix compressing subtrees */
#if 0
    struct selva_string *raw;
    struct compressed_rms *compressed;
    int err;

    err = verifyDetachableSubtree(hierarchy, node);
    if (err) {
        /* Not a valid subtree. */
#if 0
        fprintf(stderr, "%s:%d: %.*s is not a valid subtree for compression: %s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, node->id,
                selva_strerror(err));
#endif
        return NULL;
    }


    struct SelvaHierarchySubtree subtree = {
        .hierarchy = hierarchy,
        .node = node,
    };

    raw = RedisModule_SaveDataTypeToString(ctx, &subtree, HierarchySubtreeType);
    if (!raw) {
        return NULL;
    }

    compressed = rms_alloc_compressed();
    err = rms_compress(compressed, raw, cratio);
    RedisModule_FreeString(ctx, raw);
    if (err) {
        rms_free_compressed(compressed);
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to compress the subtree of %.*s: %s\n",
                  (int)SELVA_NODE_ID_SIZE, node->id,
                  selva_strerror(err));

        return NULL;
    }

    return compressed;
#endif
}

static int detach_subtree(SelvaHierarchy *hierarchy, struct SelvaHierarchyNode *node, enum SelvaHierarchyDetachedType type) {
    /* FIXME Fix detaching subtree */
#if 0
    Selva_NodeId node_id;
    Selva_NodeId *parents = NULL;
    const size_t nr_parents = SVector_Size(&node->parents);
    void *tag_compressed;
    double compression_ratio;
    int err;

    if (node->flags & SELVA_NODE_FLAGS_DETACHED) {
        SELVA_LOG(SELVA_LOGL_ERR, "Node already detached: %.*s\n",
                  (int)SELVA_NODE_ID_SIZE, node->id);
        return SELVA_HIERARCHY_EINVAL;
    }

    if (nr_parents > 0) {
        parents = alloca(nr_parents * SELVA_NODE_ID_SIZE); /* TODO Alloca maybe not great */
        copy_nodeIds(parents, &node->parents);
    }

    memcpy(node_id, node->id, SELVA_NODE_ID_SIZE);
    tag_compressed = SelvaHierarchyDetached_Store(
            node_id,
            compress_subtree(hierarchy, node, &compression_ratio),
            type);
    if (!tag_compressed) {
        return SELVA_HIERARCHY_EGENERAL;
    }

    /*
     * Now delete the compressed nodes.
     */
    err = SelvaModify_DelHierarchyNodeP(
            NULL, hierarchy, node,
            DEL_HIERARCHY_NODE_FORCE | DEL_HIERARCHY_NODE_DETACH,
            tag_compressed);
    err = err < 0 ? err : 0;
    node = NULL;
    /*
     * Note that `compressed` must not be freed as it's actually stored now in
     * the detached hierarchy for now.
     */

    /*
     * Create a new dummy node with the detached flag set.
     */
    new_detached_node(hierarchy, node_id, parents, nr_parents);

    if (!err) {
        SELVA_LOG_DBG("Compressed and detached the subtree of %.*s (cratio: %.2f:1)",
                      (int)SELVA_NODE_ID_SIZE, node_id,
                      compression_ratio);
    }

    return err;
#endif
}

static int restore_compressed_subtree(SelvaHierarchy *hierarchy, struct compressed_rms *compressed) {
    /* FIXME Fix restoring compressed subtree */
#if 0
    struct selva_string *uncompressed;
    const void *load_res;
    int err;

    err = rms_decompress(&uncompressed, compressed);
    if (err) {
        return err;
    }

    isDecompressingSubtree = 1;
    subtree_hierarchy = hierarchy;
    load_res = RedisModule_LoadDataTypeFromString(uncompressed, HierarchySubtreeType);
    isDecompressingSubtree = 0;
    selva_string_free(uncompressed);

    if (!load_res) {
        return SELVA_HIERARCHY_EINVAL;
    }

    return 0;
#endif
}

/**
 * Restore a compressed subtree back to hierarchy from the detached hierarchy subtree storage.
 * @param id can be the id of any node within a compressed subtree.
 * @returns SELVA_ENOENT if id is not a member of any detached subtree.
 */
static int restore_subtree(SelvaHierarchy *hierarchy, const Selva_NodeId id) {
    /* FIXME fix hierarchy detaching */
#if 0
    struct compressed_rms *compressed;
    int err;

    SELVA_TRACE_BEGIN(restore_subtree);

    err = SelvaHierarchyDetached_Get(hierarchy, id, &compressed, NULL);
    if (!err) {
        err = restore_compressed_subtree(hierarchy, compressed);
        if (!err) {
            rms_free_compressed(compressed);
        }
    }

    SELVA_TRACE_END(restore_subtree);

#if 0
    fprintf(stderr, "%s:%d: Restored the subtree of %.*s\n",
            __FILE__, __LINE__,
            (int)SELVA_NODE_ID_SIZE, id);
#endif

    return err;
#endif
    return 0;
}

static int _auto_compress_proc_rnd(void) {
    static unsigned int v = 300;
    static unsigned int u = 400;

    v = 36969 * (v & 65535) + (v >> 16);
    u = 18000 * (u & 65535) + (u >> 16);

    return (int)(((v << 16) + (u & 65535)) & 0x7f);
}

static void auto_compress_proc(struct RedisModuleCtx *ctx, void *data) {
    /* FIXME auto compress */
#if 0
    SELVA_TRACE_BEGIN_AUTO(auto_compress_proc);
    SelvaHierarchy *hierarchy = (struct SelvaHierarchy *)data;
    struct timespec timer_period = MSEC2TIMESPEC(selva_glob_config.hierarchy_auto_compress_period_ms);

    if (!isRdbChildRunning()) {
        const size_t n = hierarchy->inactive.nr_nodes;

        for (size_t i = 0; i < n; i++) {
            const char *node_id = hierarchy->inactive.nodes[i];
            struct SelvaHierarchyNode *node;

            if (node_id[0] == '\0') {
                break;
            }

            node = find_node_index(hierarchy, node_id);
            if (!node || node->flags & SELVA_NODE_FLAGS_DETACHED) {
                /* This should be unlikely to occur at this point. */
#if 0
                fprintf(stderr, "%s:%d Ignoring (%p) %.*s\n",
                        __FILE__, __LINE__,
                        node, (int)SELVA_NODE_ID_SIZE, node_id);
#endif
                continue;
            }

            /*
             * Note that calling detach_subtree() should also update the trx
             * struct, meaning that in case detaching the node fails, we
             * still won't see it here again any time soon.
             */
            (void)detach_subtree(hierarchy, node, SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM);
#if 0
            if (!err) {
                fprintf(stderr, "%s:%d: Auto-compressed %.*s\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, node_id);
            }
#endif
        }

        SelvaHierarchy_ClearInactiveNodeIds(hierarchy);
    } else {
        /*
         * We can't run this if a backup is still running because we share the
         * inactive nodes data structure with the backup process.
         * Add a small offset in a hope to break the accidental synchronization
         * with the RDB save process.
         */
        timer_period += 300 + _auto_compress_proc_rnd();
    }

    hierarchy->inactive.auto_compress_timer = RedisModule_CreateTimer(ctx, timer_period, auto_compress_proc, hierarchy);
#endif
}

static int load_metadata(struct selva_io *io, int encver, SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    int err;

    if (unlikely(!node)) {
        return SELVA_EINVAL;
    }

    /*
     * Note that the metadata must be loaded and saved in predefined order.
     */

    err = Edge_RdbLoad(io, encver, hierarchy, node);
    if (err) {
        return err;
    }

    /*
     * node object is currently empty because it's not created when
     * isRdbLoading() is true.
     */
    if (!SelvaObjectTypeRDBLoadTo(io, encver, SelvaObject_Init(node->_obj_data), NULL)) {
        return SELVA_ENOENT;
    }

    return 0;
}

/**
 * RDB load a node_id.
 * Should be only called by load_node().
 */
static int load_node_id(struct selva_io *io, Selva_NodeId node_id_out) {
    __selva_autofree const char *node_id = NULL;
    size_t len = 0;

    node_id = selva_io_load_str(io, &len);
    if (!node_id || len != SELVA_NODE_ID_SIZE) {
        return SELVA_HIERARCHY_EINVAL;
    }

    memcpy(node_id_out, node_id, SELVA_NODE_ID_SIZE);
    return 0;
}

static int load_detached_node(struct selva_io *io, SelvaHierarchy *hierarchy, Selva_NodeId node_id) {
    enum SelvaHierarchyDetachedType type;
    struct compressed_rms *compressed;
    SelvaHierarchyNode *node;
    int err;

    /* FIXME fix compressed strings */
#if 0
    compressed = rms_alloc_compressed();
    type = selva_io_load_signed(io);
    rms_RDBLoadCompressed(io, compressed);
#endif

    /*
     * It would be cleaner and faster to just attach this node as detached and
     * compressed directly but we don't know the nodeIds inside this compressed
     * subtree and thus can't add them to the detached hierarchy structure. We
     * could save that information separately but at the moment we don't, and
     * therefore it's easier, albeit time and memory intensive, to restore the
     * subtree first and detach it again.
     */


    /* FIXME fix compressed subtree */
#if 0
    err = restore_compressed_subtree(hierarchy, compressed);
    if (err) {
        goto out;
    }
#endif

    node = SelvaHierarchy_FindNode(hierarchy, node_id);
    if (!node) {
        err = SELVA_HIERARCHY_ENOENT;
        goto out;
    }

    err = detach_subtree(hierarchy, node, type);

out:
    /* FIXME fix compressed subtree */
#if 0
    rms_free_compressed(compressed);
#endif
    return err;
}

static int load_hierarchy_node(struct selva_io *io, int encver, SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    int err;

    /*
     * The node metadata comes right after the node_id and flags.
     */
    err = load_metadata(io, encver, hierarchy, node);
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to load hierarchy node (%.*s) metadata: %s",
                  (int)SELVA_NODE_ID_SIZE, node->id,
                  selva_strerror(err));
        return err;
    }

    /*
     * Load the ids of child nodes.
     */
    uint64_t nr_children = selva_io_load_unsigned(io);
    Selva_NodeId *children __selva_autofree = NULL;

    if (nr_children > 0) {
        children = selva_malloc(nr_children * SELVA_NODE_ID_SIZE);

        /* Create/Update children */
        for (uint64_t i = 0; i < nr_children; i++) {
            Selva_NodeId child_id;

            err = load_node_id(io, child_id);
            if (err) {
                SELVA_LOG(SELVA_LOGL_CRIT, "Invalid child node_id: %s",
                          selva_strerror(err));
                return err;
            }

            if (isDecompressingSubtree) {
                /* FIXME Implement decompressing hierarchy */
#if 0
                SelvaHierarchyDetached_RemoveNode(hierarchy, child_id);
#endif
            }

            err = SelvaModify_AddHierarchy(hierarchy, child_id, 0, NULL, 0, NULL);
            if (err < 0) {
                SELVA_LOG(SELVA_LOGL_CRIT, "Unable to rebuild the hierarchy: %s",
                          selva_strerror(err));
                return err;
            }

            memcpy(children + i, child_id, SELVA_NODE_ID_SIZE);
        }
    }

    /*
     * Insert children of the node.
     */
    err = SelvaModify_AddHierarchyP(hierarchy, node, 0, NULL, nr_children, children);
    if (err < 0) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Unable to rebuild the hierarchy: %s",
                  selva_strerror(err));
        return err;
    }

    return 0;
}

/**
 * RDB load a node and its children.
 * Should be only called by load_tree().
 */
static int load_node(struct selva_io *io, int encver, SelvaHierarchy *hierarchy, Selva_NodeId node_id) {
    SelvaHierarchyNode *node;
    int err;

    if (isDecompressingSubtree) {
        /* FIXME Implement decompressing hierarchy */
#if 0
        SelvaHierarchyDetached_RemoveNode(hierarchy, node_id);
#endif
    }

    /*
     * Upsert the node.
     */
    err = SelvaHierarchy_UpsertNode(hierarchy, node_id, &node);
    if (err && err != SELVA_HIERARCHY_EEXIST) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to upsert %.*s: %s",
                  (int)SELVA_NODE_ID_SIZE, node_id,
                  selva_strerror(err));
        return err;
    }

    if (encver > 3) {
        node->flags = selva_io_load_unsigned(io);
    }

    if (node->flags & SELVA_NODE_FLAGS_DETACHED && !isDecompressingSubtree) {
        /*
         * This node and its subtree was compressed.
         * In this case we are supposed to load the subtree as detached and
         * keep it compressed.
         * SELVA_NODE_FLAGS_DETACHED should never be set if
         * isDecompressingSubtree is set but the code looks cleaner this way.
         */
        err = load_detached_node(io, hierarchy, node_id);
    } else {
        err = load_hierarchy_node(io, encver, hierarchy, node);
    }

    return err;
}

/**
 * Load a node hierarchy from io.
 * NODE_ID1 | FLAGS | METADATA | NR_CHILDREN | CHILD_ID_0,..
 * NODE_ID2 | FLAGS | METADATA | NR_CHILDREN | ...
 * HIERARCHY_RDB_EOF
 */
static int load_tree(struct selva_io *io, int encver, SelvaHierarchy *hierarchy) {
    while (1) {
        Selva_NodeId node_id;
        int err;

        err = load_node_id(io, node_id);
        if (err) {
            SELVA_LOG(SELVA_LOGL_CRIT, "Failed to load the next nodeId: %s",
                      selva_strerror(err));
            return SELVA_HIERARCHY_EINVAL;
        }

        /*
         * If it's EOF there are no more nodes for this hierarchy.
         */
        if (!memcmp(node_id, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE)) {
            break;
        }

        err = load_node(io, encver, hierarchy, node_id);
        if (err) {
            return err;
        }
    }

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Update depths on a single pass to save time.
     */
    struct SVectorIterator it;
    SelvaHierarchyNode *head;

    SVector_ForeachBegin(&it, &hierarchy->heads);
    while ((head = SVector_Foreach(&it))) {
        updateDepth(hierarchy, head);
    }
#endif

    return 0;
}

static void *Hierarchy_RDBLoad(struct selva_io *io, int encver) {
    SelvaHierarchy *hierarchy;
    int err;

    if (encver > HIERARCHY_ENCODING_VERSION) {
        SELVA_LOG(SELVA_LOGL_CRIT, "selva_hierarchy encoding version %d not supported", encver);
        return NULL;
    }

    /*
     * TODO This should probably replace the main_hierarchy.
     */
    hierarchy = SelvaModify_NewHierarchy();
    if (!hierarchy) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to create a new hierarchy");
        return NULL;
    }

    if (encver >= 5) {
        if (!SelvaObjectTypeRDBLoadTo(io, encver, SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy), NULL)) {
            SELVA_LOG(SELVA_LOGL_CRIT, "Failed to node types");
            return NULL;
        }
    }

    err = EdgeConstraint_RdbLoad(io, encver, &hierarchy->edge_field_constraints);
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to load the dynamic constraints: %s",
                  selva_strerror(err));
        goto error;
    }

    err = load_tree(io, encver, hierarchy);
    if (err) {
        goto error;
    }

    return hierarchy;
error:
    SelvaModify_DestroyHierarchy(hierarchy);

    return NULL;
}

static void save_detached_node(struct selva_io *io, SelvaHierarchy *hierarchy, const Selva_NodeId id) {
    struct compressed_rms *compressed;
    enum SelvaHierarchyDetachedType type;
    int err;

    /*
     * Technically we point to the same compressed subtree multiple times in the
     * detached hierarchy. However, we should only point to it once in the
     * actual hierarchy as these are proper subtrees. This means that we'll only
     * store the compressed subtree once. The down side is that the only way to
     * rebuild the SelvaHierarchyDetached structure is by decompressing the
     * subtrees temporarily.
     */

    /* FIXME fix hiereachy detaching */
#if 0
    err = SelvaHierarchyDetached_Get(hierarchy, id, &compressed, &type);
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to save a compressed subtree: %s",
                  selva_strerror(err));
        return;
    }

    selva_io_save_signed(io, type);
    rms_RDBSaveCompressed(io, compressed);
#endif
}

static void save_metadata(struct selva_io *io, SelvaHierarchyNode *node) {
    /*
     * Note that the metadata must be loaded and saved in a predefined order.
     */

    Edge_RdbSave(io, node);
    SelvaObjectTypeRDBSave(io, GET_NODE_OBJ(node), NULL);
}

/**
 * Save a node.
 * Used by Hierarchy_RDBSave() when doing an rdb dump.
 */
static int HierarchyRDBSaveNode(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct HierarchyRDBSaveNode *args = (struct HierarchyRDBSaveNode *)arg;
    struct selva_io *io = args->io;

    selva_io_save_str(io, node->id, SELVA_NODE_ID_SIZE);
    selva_io_save_unsigned(io, node->flags);

    if (node->flags & SELVA_NODE_FLAGS_DETACHED) {
        save_detached_node(io, hierarchy, node->id);
    } else {
        save_metadata(io, node);
        selva_io_save_unsigned(io, SVector_Size(&node->children));
    }

    return 0;
}

/**
 * Save a node from a subtree.
 * Used by Hierarchy_SubtreeRDBSave() when saving a subtree into a string.
 * This function should match with HierarchyRDBSaveNode() but we don't want
 * to do save_detached() here.
 */
static int HierarchyRDBSaveSubtreeNode(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        struct SelvaHierarchyNode *node,
        void *arg) {
    struct selva_io *io = (struct selva_io *)arg;

    selva_io_save_str(io, node->id, SELVA_NODE_ID_SIZE);
    selva_io_save_unsigned(io, node->flags & ~SELVA_NODE_FLAGS_DETACHED);
    save_metadata(io, node);
    selva_io_save_unsigned(io, SVector_Size(&node->children));

    return 0;
}

static void HierarchyRDBSaveChild(
        struct RedisModuleCtx *ctx __unused,
        struct SelvaHierarchy *hierarchy __unused,
        const struct SelvaHierarchyTraversalMetadata *metadata __unused,
        struct SelvaHierarchyNode *child,
        void *arg) {
    struct selva_io *io = (struct selva_io *)arg;

    /*
     * We don't need to care here whether the node is detached because the
     * node callback is the only callback touching the node data. Here we
     * are only interested in saving the child ids.
     */

    selva_io_save_str(io, child->id, SELVA_NODE_ID_SIZE);
}

static void save_hierarchy(struct selva_io *io, SelvaHierarchy *hierarchy) {
    struct HierarchyRDBSaveNode args = {
        .io = io,
    };
    const struct SelvaHierarchyCallback cb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = HierarchyRDBSaveNode,
        .node_arg = &args,
        .child_cb = HierarchyRDBSaveChild,
        .child_arg = io,
        .flags = SELVA_HIERARCHY_CALLBACK_FLAGS_INHIBIT_RESTORE,
    };

    (void)full_dfs(hierarchy, &cb);
    selva_io_save_str(io, HIERARCHY_RDB_EOF, sizeof(HIERARCHY_RDB_EOF));
}

static void Hierarchy_RDBSave(struct selva_io *io, void *value) {
    SelvaHierarchy *hierarchy = (SelvaHierarchy *)value;

    /*
     * Serialization format:
     * TYPE_MAP
     * EDGE_CONSTRAINTS
     * NODE_ID1 | FLAGS | METADATA | NR_CHILDREN | CHILD_ID_0,..
     * NODE_ID2 | FLAGS | METADATA | NR_CHILDREN | ...
     * HIERARCHY_RDB_EOF
     */
    isRdbSaving = 1;
    SelvaObjectTypeRDBSave(io, SELVA_HIERARCHY_GET_TYPES_OBJ(hierarchy), NULL);
    EdgeConstraint_RdbSave(io, &hierarchy->edge_field_constraints);
    save_hierarchy(io, hierarchy);
    isRdbSaving = 0;
}

static int load_nodeId(struct selva_io *io, Selva_NodeId nodeId) {
    __selva_autofree const char *buf = NULL;
    size_t len;

    buf = selva_io_load_str(io, &len);
    if (!buf || len != SELVA_NODE_ID_SIZE) {
        return SELVA_HIERARCHY_EINVAL;
    }

    memcpy(nodeId, buf, SELVA_NODE_ID_SIZE);
    return 0;
}

/**
 * DO NOT CALL.
 * Load a subtree from the RDB serialization format back into the hierarchy.
 * This function should never be called directly.
 */
static void *Hierarchy_SubtreeRDBLoad(struct selva_io *io, int encver) {
    SelvaHierarchy *hierarchy = subtree_hierarchy;
    Selva_NodeId nodeId;
    int err;

    /*
     * 1. Load encoding version
     * 2. Read nodeId
     * 3. Load the children normally
     */

    encver = selva_io_load_signed(io);
    if (encver > HIERARCHY_ENCODING_VERSION) {
        SELVA_LOG(SELVA_LOGL_CRIT, "selva_hierarchy encoding version %d not supported", encver);
        return NULL;
    }

    /*
     * Read nodeId.
     */
    if (load_nodeId(io, nodeId)) {
        return NULL;
    }

    /* FIXME Implement decompressing hierarchy */
#if 0
    SelvaHierarchyDetached_RemoveNode(hierarchy, nodeId);
#endif

    err = load_tree(io, encver, hierarchy);
    if (err) {
        return NULL;
    }

    return (void *)1;
}

/**
 * DO NOT CALL.
 * Serialize a subtree of a hierarchy.
 * This function should never be called directly.
 */
static void Hierarchy_SubtreeRDBSave(struct selva_io *io, void *value) {
    struct SelvaHierarchySubtree *subtree = (struct SelvaHierarchySubtree *)value;
    SelvaHierarchy *hierarchy = subtree->hierarchy;
    struct SelvaHierarchyNode *node = subtree->node;
    const struct SelvaHierarchyCallback cb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = HierarchyRDBSaveSubtreeNode,
        .node_arg = io,
        .child_cb = HierarchyRDBSaveChild,
        .child_arg = io,
    };

    /*
     * Save encoding version.
     * This needs to be stored separately because we need to be able to read
     * compressed subtrees from the disk and those files don't contain the
     * encoding version and Redis gives us version 0 on read.
     */
    selva_io_save_signed(io, HIERARCHY_ENCODING_VERSION);

    /* Save nodeId. */
    selva_io_save_str(io, node->id, SELVA_NODE_ID_SIZE);

    /*
     * Save the children.
     */
    (void)dfs(hierarchy, node, RELATIONSHIP_CHILD, &cb);
    selva_io_save_str(io, HIERARCHY_RDB_EOF, sizeof(HIERARCHY_RDB_EOF));
}

void HierarchyTypeFree(void *value) {
    SelvaHierarchy *hierarchy = (SelvaHierarchy *)value;

    SelvaModify_DestroyHierarchy(hierarchy);
}

/*
 * SELVA.HIERARCHY.DEL HIERARCHY_KEY FLAGS [NODE_ID1[, NODE_ID2, ...]]
 * If no NODE_IDs are given then nothing will be deleted.
 */
int SelvaHierarchy_DelNodeCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    if (argc < 3) {
        return selva_send_error_arity(resp);
    }

    size_t flags_len;
    const char *flags_str = selva_string_to_str(argv[2], &flags_len);
    enum SelvaModify_DelHierarchyNodeFlag flags = 0;
    SelvaHierarchy *hierarchy = main_hierarchy;

    for (size_t i = 0; i < flags_len; i++) {
        flags |= flags_str[i] == 'F' ? DEL_HIERARCHY_NODE_FORCE : 0;
        flags |= flags_str[i] == 'I' ? DEL_HIERARCHY_NODE_REPLY_IDS : 0;
    }

    if ((flags & DEL_HIERARCHY_NODE_REPLY_IDS) != 0) {
        selva_send_array(resp, -1);
    }

    long long nr_deleted = 0;
    for (int i = 3; i < argc; i++) {
        Selva_NodeId nodeId;
        int res;

        selva_string2node_id(nodeId, argv[i]);
        res = SelvaModify_DelHierarchyNode(resp, hierarchy, nodeId, flags);
        if (res >= 0) {
            nr_deleted += res;
        } else {
            /* TODO How to handle the error correctly? */
            /* DEL_HIERARCHY_NODE_REPLY_IDS would allow us to send errors. */
            if (res != SELVA_HIERARCHY_ENOENT) {
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to delete the node %.*s: %s\n",
                          (int)SELVA_NODE_ID_SIZE, nodeId,
                          selva_strerror(res));
            }
        }
    }

    if ((flags & DEL_HIERARCHY_NODE_REPLY_IDS) != 0) {
        selva_send_array_end(resp);
    } else {
        selva_send_ll(resp, nr_deleted);
    }
    /* FIXME Replicate */
#if 0
    RedisModule_ReplicateVerbatim(ctx);
#endif
    SelvaSubscriptions_SendDeferredEvents(hierarchy);

    return 0;
}

int SelvaHierarchy_HeadsCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    if (argc != 2) {
        return selva_send_error_arity(resp);
    }

    const SelvaHierarchy *hierarchy = main_hierarchy;
    struct SVectorIterator it;
    const SelvaHierarchyNode *node;

    selva_send_array(resp, SVector_Size(&hierarchy->heads));

    SVector_ForeachBegin(&it, &hierarchy->heads);
    while ((node = SVector_Foreach(&it))) {
        selva_send_str(resp, node->id, Selva_NodeIdLen(node->id));
    }

    return 0;
}

int SelvaHierarchy_ParentsCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    if (argc != 3) {
        return selva_send_error_arity(resp);
    }

    SelvaHierarchy *hierarchy = main_hierarchy;
    Selva_NodeId nodeId;

    /*
     * Find the node.
     */
    selva_string2node_id(nodeId, argv[2]);
    const SelvaHierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return selva_send_error(resp, SELVA_HIERARCHY_ENOENT, NULL, 0);
    }

    struct SVectorIterator it;
    const SelvaHierarchyNode *parent;
    const SVector *parents;

#if HIERARCHY_SORT_BY_DEPTH
    SVECTOR_AUTOFREE(parents_d);

    if (unlikely(!SVector_Clone(&parents_d, &node->parents, SVector_HierarchyNode_depth_compare))) {
        return selva_send_error(resp, SELVA_HIERARCHY_ENOMEM, NULL 0);
    }

    parents = &parents_d;
#else
    parents = &node->parents;
#endif

    selva_send_array(resp, SVector_Size(parents));

    SVector_ForeachBegin(&it, parents);
    while ((parent = SVector_Foreach(&it))) {
        selva_send_str(resp, parent->id, Selva_NodeIdLen(parent->id));
    }

    return 0;
}

int SelvaHierarchy_ChildrenCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    if (argc != 3) {
        return selva_send_error_arity(resp);
    }

    SelvaHierarchy *hierarchy = main_hierarchy;
    Selva_NodeId nodeId;

    /*
     * Find the node.
     */
    selva_string2node_id(nodeId, argv[2]);
    const SelvaHierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return selva_send_error(resp, SELVA_HIERARCHY_ENOENT, NULL, 0);
    }

    selva_send_array(resp, SVector_Size(&node->children));

    struct SVectorIterator it;
    const SelvaHierarchyNode *child;

    SVector_ForeachBegin(&it, &node->children);
    while((child = SVector_Foreach(&it))) {
        selva_send_str(resp, child->id, Selva_NodeIdLen(child->id));
    }

    return 0;
}

int SelvaHierarchy_EdgeListCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    if (argc != 3 && argc != 4) {
        return selva_send_error_arity(resp);
    }

    SelvaHierarchy *hierarchy = main_hierarchy;
    Selva_NodeId nodeId;

    /*
     * Find the node.
     */
    selva_string2node_id(nodeId, argv[2]);
    SelvaHierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return selva_send_error(resp, SELVA_HIERARCHY_ENOENT, NULL, 0);
    }

    struct SelvaObject *obj = node->metadata.edge_fields.edges;

    if (!obj) {
        /* No custom edges set. */
        return selva_send_array(resp, 0);
    }

    const struct selva_string *key_name = argc == 4 ? argv[3] : NULL;
    if (key_name) {
        int err;

        err = SelvaObject_GetObject(obj, key_name, &obj);
        if (err) {
            return selva_send_error(resp, err, NULL, 0);
        }
    }

    SelvaObject_ReplyWithObject(resp, NULL, obj, NULL, 0);

    return 0;
}

/*
 * Get edges of an edge field.
 *
 * Reply format:
 * [
 *   constraint_id,
 *   nodeId1,
 *   nodeId2,
 * ]
 */
int SelvaHierarchy_EdgeGetCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    if (argc != 4) {
        return selva_send_error_arity(resp);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = main_hierarchy;
    Selva_NodeId nodeId;
    const SelvaHierarchyNode *node;

    /*
     * Find the node.
     */
    selva_string2node_id(nodeId, argv[2]);
    node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return selva_send_error(resp, SELVA_HIERARCHY_ENOENT, NULL, 0);
    }

    const struct selva_string *field_name = argv[3];
    TO_STR(field_name);
    const struct EdgeField *edge_field;

    edge_field = Edge_GetField(node, field_name_str, field_name_len);
    if (!edge_field) {
        return selva_send_null(resp);
    }

    const struct SVector *arcs = &edge_field->arcs;
    struct SVectorIterator it;
    const SelvaHierarchyNode *dst;

    selva_send_array(resp, 1 + SVector_Size(arcs));
    selva_send_ll(resp, edge_field->constraint ? edge_field->constraint->constraint_id : EDGE_FIELD_CONSTRAINT_ID_DEFAULT);

    SVector_ForeachBegin(&it, arcs);
    while ((dst = SVector_Foreach(&it))) {
        selva_send_str(resp, dst->id, Selva_NodeIdLen(dst->id));
    }

    return 0;
}

/*
 * Get metadata of an edge.
 *
 * Reply format:
 * SelvaObject
 */
int SelvaHierarchy_EdgeGetMetadataCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    Selva_NodeId src_node_id;
    const struct selva_string *field_name;
    Selva_NodeId dst_node_id;
    int err;

    if (argc != 5) {
        return selva_send_error_arity(resp);
    }

    selva_string2node_id(src_node_id, argv[2]);
    field_name = argv[3];
    selva_string2node_id(dst_node_id, argv[4]);
    TO_STR(field_name);

    SelvaHierarchy *hierarchy = main_hierarchy;

    /*
     * Find the node.
     */
    const SelvaHierarchyNode *src_node = SelvaHierarchy_FindNode(hierarchy, src_node_id);
    if (!src_node) {
        return selva_send_error(resp, SELVA_HIERARCHY_ENOENT, NULL, 0);
    }

    struct EdgeField *edge_field;
    edge_field = Edge_GetField(src_node, field_name_str, field_name_len);
    if (!edge_field) {
        return selva_send_errorf(resp, SELVA_ENOENT, "Edge field not found");
    }

    struct SelvaObject *edge_metadata;
    err = Edge_GetFieldEdgeMetadata(edge_field, dst_node_id, 0, &edge_metadata);
    if (err == SELVA_ENOENT) {
        return selva_send_null(resp);
    } else if (err) { /* Also if the edge doesn't exist. */
        return selva_send_error(resp, err, NULL, 0);
    }

    SelvaObject_ReplyWithObject(resp, NULL, edge_metadata, NULL, SELVA_OBJECT_REPLY_BINUMF_FLAG);

    return 0;
}

int SelvaHierarchy_CompressCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    enum SelvaHierarchyDetachedType type = SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM;
    int err;

    if (argc != 3 && argc != 4) {
        return selva_send_error_arity(resp);
    }

    if (argc == 4) {
        static const struct SelvaArgParser_EnumType types[] = {
            {
                .name = "mem",
                .id = SELVA_HIERARCHY_DETACHED_COMPRESSED_MEM,
            },
            {
                .name = "disk",
                .id = SELVA_HIERARCHY_DETACHED_COMPRESSED_DISK,
            },
            {
                .name = NULL,
                .id = 0,
            }
        };

        err = SelvaArgParser_Enum(types, argv[3]);
        if (err < 0) {
            return selva_send_error(resp, err, "Type", 4);
        }
        type = err;
    }

    SelvaHierarchy *hierarchy = main_hierarchy;
    Selva_NodeId nodeId;

    /*
     * Find the node.
     */
    selva_string2node_id(nodeId, argv[2]);
    SelvaHierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return selva_send_error(resp, SELVA_HIERARCHY_ENOENT, NULL, 0);
    }

    err = detach_subtree(hierarchy, node, type);
    if (err) {
        return selva_send_error(resp, err, NULL, 0);
    }

    selva_send_ll(resp, 1);
    /* FIXME Replicate */
#if 0
    RedisModule_ReplicateVerbatim(ctx);
#endif

    return 0;
}

int SelvaHierarchy_ListCompressedCommand(struct selva_server_response_out *resp, struct selva_string **argv, int argc) {
    SelvaObject_Iterator *it;
    const char *id;

    if (argc != 2) {
        return selva_send_error_arity(resp);
    }

    SelvaHierarchy *hierarchy = main_hierarchy;

    if (!hierarchy->detached.obj) {
        return selva_send_array(resp, 0);
    }

    selva_send_array(resp, SelvaObject_Len(hierarchy->detached.obj, NULL));
    it = SelvaObject_ForeachBegin(hierarchy->detached.obj);
    while ((id = SelvaObject_ForeachKey(hierarchy->detached.obj, &it))) {
        selva_send_str(resp, id, strlen(id));
    }

    return 0;
}

int SelvaHierarchy_VerCommand(struct selva_server_response_out *resp, struct selva_string **argv __unused, int argc) {
    if (argc != 1) {
        return selva_send_error_arity(resp);
    }

    selva_send_array(resp, 6);

    selva_send_str(resp, "running", 7);
    selva_send_str(resp, selva_db_version, strlen(selva_db_version));

    selva_send_str(resp, "created", 7);
    if (selva_db_version_info.created_with) {
        selva_send_string(resp, selva_db_version_info.created_with);
    } else {
        selva_send_null(resp);
    }

    selva_send_str(resp, "updated", 7);
    if (selva_db_version_info.updated_with) {
        selva_send_string(resp, selva_db_version_info.updated_with);
    } else {
        selva_send_null(resp);
    }

    return 0;
}

static int SelvaVersion_AuxLoad(struct selva_io *io, int encver __unused, int when __unused) {
    selva_db_version_info.created_with = selva_io_load_string(io);
    selva_db_version_info.updated_with = selva_io_load_string(io);

    SELVA_LOG(SELVA_LOGL_INFO,
              "Selva hierarchy version info created_with: %s updated_with: %s",
              selva_string_to_str(selva_db_version_info.created_with, NULL),
              selva_string_to_str(selva_db_version_info.updated_with, NULL));

    return 0;
}

static void SelvaVersion_AuxSave(struct selva_io *io, int when __unused) {
    const size_t len = strlen(selva_db_version);

    if (selva_db_version_info.created_with) {
        selva_io_save_string(io, selva_db_version_info.created_with);
    } else {
        selva_io_save_str(io, selva_db_version, len);
    }
    selva_io_save_str(io, selva_db_version, len);
}

static int Hierarchy_OnLoad(struct RedisModuleCtx *ctx) {
    /* FIXME Load & save */
#if 0
    RedisModuleTypeMethods mtm = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = Hierarchy_RDBLoad,
        .rdb_save = Hierarchy_RDBSave,
        .aof_rewrite = NULL,
        .free = HierarchyTypeFree,
        .aux_load = SelvaVersion_AuxLoad,
        .aux_save = SelvaVersion_AuxSave,
        .aux_save_triggers = REDISMODULE_AUX_BEFORE_RDB,
    };
    RedisModuleTypeMethods ztm = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = Hierarchy_SubtreeRDBLoad,
        .rdb_save = Hierarchy_SubtreeRDBSave,
    };

    HierarchyType = RedisModule_CreateDataType(ctx, "hierarchy", HIERARCHY_ENCODING_VERSION, &mtm);
    if (HierarchyType == NULL) {
        /* TODO */
        return SELVA_HIERARCHY_EGENERAL;
    }

    HierarchySubtreeType = RedisModule_CreateDataType(ctx, "hisubtree", HIERARCHY_ENCODING_VERSION, &ztm);
    if (HierarchyType == NULL) {
        /* TODO */
        return SELVA_HIERARCHY_EGENERAL;
    }
#endif

    /*
     * Register commands.
     */
    selva_mk_command(23, "hierarchy.del", SelvaHierarchy_DelNodeCommand);
    selva_mk_command(24, "hierarchy.heads", SelvaHierarchy_HeadsCommand);
    selva_mk_command(25, "hierarchy.parents", SelvaHierarchy_ParentsCommand);
    selva_mk_command(26, "hierarchy.children", SelvaHierarchy_ChildrenCommand);
    selva_mk_command(27, "hierarchy.edgeList", SelvaHierarchy_EdgeListCommand);
    selva_mk_command(28, "hierarchy.edgeGet", SelvaHierarchy_EdgeGetCommand);
    selva_mk_command(29, "hierarchy.edgeGetMetadata", SelvaHierarchy_EdgeGetMetadataCommand);
    selva_mk_command(30, "hierarchy.compress", SelvaHierarchy_CompressCommand);
    selva_mk_command(31, "hierarchy.listCompressed", SelvaHierarchy_ListCompressedCommand);
    selva_mk_command(32, "hierarchy.ver", SelvaHierarchy_VerCommand);

    main_hierarchy = SelvaModify_NewHierarchy();
    if (!main_hierarchy) { /* TODO Can this even fail? */
        return SELVA_HIERARCHY_ENOMEM;
    }

    return 0;
}
SELVA_ONLOAD(Hierarchy_OnLoad);
