#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "redismodule.h"
#include "auto_free.h"
#include "selva.h"
#include "alias.h"
#include "async_task.h"
#include "cdefs.h"
#include "rms.h"
#include "errors.h"
#include "edge.h"
#include "modify.h"
#include "rpn.h"
#include "config.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_trace.h"
#include "find_index.h"
#include "selva_set.h"
#include "traversal.h"
#include "subscriptions.h"
#include "svector.h"
#include "hierarchy.h"
#include "hierarchy_detached.h"

/**
 * Selva module version tracking.
 * This is used to track the Selva module version used to create and modify the
 * hierarchy that was serialized and later deserialized.
 */
struct SelvaDbVersionInfo {
    RedisModuleString *created_with;
    RedisModuleString *updated_with;
} selva_db_version_info;

/**
 * The core type of Selva hierarchy.
 */
typedef struct SelvaHierarchyNode {
    Selva_NodeId id; /* Must be first. */
    struct {
        /**
         * Detached node.
         * When set this is the head of a compressed subtree stored in
         * hierarchy_detached. Some information has been removed from the node
         * and the subtree must be restored to make this node usable.
         */
        uint8_t detached : 1;
    } flags;
    struct trx trx_label;
#if HIERARCHY_SORT_BY_DEPTH
    ssize_t depth;
#endif
    struct SelvaObject *obj;
    struct SelvaHierarchyMetadata metadata;
    SVector parents;
    SVector children;
    RB_ENTRY(SelvaHierarchyNode) _index_entry;
} SelvaHierarchyNode;

/**
 * Filter struct used for RB searches.
 */
typedef struct SelvaHierarchySearchFilter {
    Selva_NodeId id;
} SelvaHierarchySearchFilter;

enum SelvaHierarchyNode_Relationship {
    RELATIONSHIP_PARENT,
    RELATIONSHIP_CHILD,
};

/**
 * Structure for traversal cb of verifyDetachableSubtree().
 */
struct verifyDetachableSubtree {
    int err; /*!< Set to a Selva error if the subtree doesn't verify. */
    struct trx trx_cur; /*!< This is used to check if the children of node form a true subtree. */
    SelvaHierarchyNode *head;
};

/**
 * Type used by subtree serialization traversal functions.
 */
struct SelvaHierarchySubtree {
    SelvaHierarchy *hierarchy;
    struct SelvaHierarchyNode *node;
};

static void SelvaModify_DestroyNode(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node);
static void removeRelationships(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        enum SelvaHierarchyNode_Relationship rel);
RB_PROTOTYPE_STATIC(hierarchy_index_tree, SelvaHierarchyNode, _index_entry, SelvaHierarchyNode_Compare)
static int detach_subtree(RedisModuleCtx *ctx, SelvaHierarchy *hierarchy, struct SelvaHierarchyNode *node);
static int restore_subtree(SelvaHierarchy *hierarchy, const Selva_NodeId id);

/* Node metadata constructors. */
SET_DECLARE(selva_HMCtor, SelvaHierarchyMetadataConstructorHook);
/* Node metadata destructors. */
SET_DECLARE(selva_HMDtor, SelvaHierarchyMetadataDestructorHook);

__nonstring static const Selva_NodeId HIERARCHY_RDB_EOF;
static RedisModuleType *HierarchyType;
static RedisModuleType *HierarchySubtreeType;

SELVA_TRACE_HANDLE(find_inmem);
SELVA_TRACE_HANDLE(find_detached);
SELVA_TRACE_HANDLE(restore_subtree);

/**
 * A pointer to the hierarchy subtree being loaded.
 * Redis doesn't allow passing any pointers when loading a stringified RDB so a
 * global variable is needed for Hierarchy_SubtreeRDBLoad().
 */
static SelvaHierarchy *subtree_hierarchy;
static int isDecompressingSubtree;

static int isRdbLoading(RedisModuleCtx *ctx) {
     return !!(REDISMODULE_CTX_FLAGS_LOADING & RedisModule_GetContextFlags(ctx) || isDecompressingSubtree);
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

SelvaHierarchy *SelvaModify_NewHierarchy(RedisModuleCtx *ctx) {
    SelvaHierarchy *hierarchy;

    hierarchy = RedisModule_Calloc(1, sizeof(*hierarchy));
    if (unlikely(!hierarchy)) {
        goto fail;
    }

    RB_INIT(&hierarchy->index_head);
    if (unlikely(!SVector_Init(&hierarchy->heads, 1, SVector_HierarchyNode_id_compare))) {
#if MEM_DEBUG
        memset(hierarchy, 0, sizeof(*hierarchy));
#endif
        RedisModule_Free(hierarchy);
        hierarchy = NULL;
        goto fail;
    }

    Edge_InitEdgeFieldConstraints(&hierarchy->edge_field_constraints);
    Selva_Subscriptions_InitHierarchy(hierarchy);

    if (SelvaFindIndex_Init(ctx, hierarchy)) {
        SelvaModify_DestroyHierarchy(hierarchy);
        hierarchy = NULL;
        goto fail;
    }

    if (unlikely(SelvaModify_SetHierarchy(isRdbLoading(ctx) ? NULL : ctx, hierarchy, ROOT_NODE_ID, 0, NULL, 0, NULL) < 0)) {
        SelvaModify_DestroyHierarchy(hierarchy);
        hierarchy = NULL;
        goto fail;
    }

fail:
    return hierarchy;
}

void SelvaModify_DestroyHierarchy(SelvaHierarchy *hierarchy) {
    SelvaHierarchyNode *node;
    SelvaHierarchyNode *next;

    for (node = RB_MIN(hierarchy_index_tree, &hierarchy->index_head); node != NULL; node = next) {
        next = RB_NEXT(hierarchy_index_tree, &hierarchy->index_head, node);
        RB_REMOVE(hierarchy_index_tree, &hierarchy->index_head, node);
        SelvaModify_DestroyNode(NULL, hierarchy, node);
    }

    /*
     * Note that ctx can be NULL because we are freeing the whole hierarchy
     * which will destroy all hierarchy nodes and thus all the marker pointers
     * will be gone anyway.
     */
    SelvaSubscriptions_DestroyAll(NULL, hierarchy);
    /*
     * If SelvaSubscriptions_DestroyAll() is ran first the we don't need to
     * bother about cleaning up subscriptions used by the indexing and thus
     * hopefully the deinit doesn't need a RedisModuleCtx.
     */
    SelvaFindIndex_Deinit(hierarchy);

    Edge_DeinitEdgeFieldConstraints(&hierarchy->edge_field_constraints);

    SVector_Destroy(&hierarchy->heads);
#if MEM_DEBUG
    memset(hierarchy, 0, sizeof(*hierarchy));
#endif
    RedisModule_Free(hierarchy);
}

SelvaHierarchy *SelvaModify_OpenHierarchy(RedisModuleCtx *ctx, RedisModuleString *key_name, int mode) {
    SelvaHierarchy *hierarchy = NULL;
    RedisModuleKey *key;
    int type;

    key = RedisModule_OpenKey(ctx, key_name, mode);
    type = RedisModule_KeyType(key);

    if (type != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != HierarchyType) {
        RedisModule_CloseKey(key);
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);

        return NULL;
    }

    /* Create an empty value object if the key is currently empty. */
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        if ((mode & REDISMODULE_WRITE) == REDISMODULE_WRITE) {
            hierarchy = SelvaModify_NewHierarchy(ctx);
            if (!hierarchy) {
                replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOMEM);
            }

            RedisModule_ModuleTypeSetValue(key, HierarchyType, hierarchy);
        } else {
            replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
        }
    } else {
        hierarchy = RedisModule_ModuleTypeGetValue(key);
        if (!hierarchy) {
            replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
        }
    }

    return hierarchy;
}

static int create_node_object(SelvaHierarchyNode *node) {
    RedisModuleString *node_name;
    int err;

    node_name = RedisModule_CreateStringPrintf(NULL, "%.*s", (int)SELVA_NODE_ID_SIZE, node->id);
    if (unlikely(!node_name)) {
        return SELVA_HIERARCHY_ENOMEM;
    }

    node->obj = SelvaObject_New();
    if (!node->obj) {
        return SELVA_HIERARCHY_ENOMEM;
    }

    err = SelvaObject_SetStringStr(node->obj, SELVA_ID_FIELD, sizeof(SELVA_ID_FIELD) - 1, node_name);
    if (err) {
        RedisModule_FreeString(NULL, node_name);
        return err;
    }

    /* Set the type for root. */
    if (!memcmp(node->id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE)) {
        RedisModuleString *type;

        type = RedisModule_CreateStringPrintf(NULL, "root");
        if (!type) {
            return SELVA_ENOMEM;
        }

        err = SelvaObject_SetStringStr(node->obj, SELVA_TYPE_FIELD, sizeof(SELVA_TYPE_FIELD) - 1, type);
        if (err) {
            RedisModule_FreeString(NULL, type);
            return err;
        }
    }

    return 0;
}

/**
 * Create a new node.
 */
static SelvaHierarchyNode *newNode(RedisModuleCtx *ctx, const Selva_NodeId id) {
    SelvaHierarchyNode *node;

    node = RedisModule_Calloc(1, sizeof(SelvaHierarchyNode));
    if (unlikely(!node)) {
        return NULL;
    };

#if 0
    fprintf(stderr, "%s:%d: Creating node %.*s\n",
            __FILE__, __LINE__,
            (int)SELVA_NODE_ID_SIZE, id);
#endif

    if (unlikely(!SVector_Init(&node->parents,  selva_glob_config.hierarchy_initial_vector_len, SVector_HierarchyNode_id_compare) ||
                 !SVector_Init(&node->children, selva_glob_config.hierarchy_initial_vector_len, SVector_HierarchyNode_id_compare))) {
        SVector_Destroy(&node->parents);
        SVector_Destroy(&node->children);
#if MEM_DEBUG
        memset(node, 0, sizeof(*node));
#endif
        RedisModule_Free(node);
        return NULL;
    }

    memcpy(node->id, id, SELVA_NODE_ID_SIZE);

    /* The SelvaObject is created elsewhere if we are loading and ctx is not set. */
    if (likely(ctx)) {
        int err;

        err = create_node_object(node);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to create a hash for \"%.*s\": %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, id,
                    selvaStrError[-err]);
            /*
             * RFE:
             * This might just work even without the node so we don't fail hard.
             */
        }
    }

    SelvaHierarchyMetadataConstructorHook **metadata_ctor_p;

    SET_FOREACH(metadata_ctor_p, selva_HMCtor) {
        SelvaHierarchyMetadataConstructorHook *ctor = *metadata_ctor_p;
        ctor(node->id, &node->metadata);
    }

    return node;
}

static void SelvaModify_DestroyNode(RedisModuleCtx *ctx, SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    SelvaHierarchyMetadataDestructorHook **dtor_p;

    /* Don't pass ctx when loading. */
    ctx = isRdbLoading(ctx) ? NULL : ctx;

    SET_FOREACH(dtor_p, selva_HMDtor) {
        SelvaHierarchyMetadataDestructorHook *dtor = *dtor_p;
        dtor(ctx, hierarchy, node, &node->metadata);
    }

    SVector_Destroy(&node->parents);
    SVector_Destroy(&node->children);
#if MEM_DEBUG
    memset(node, 0, sizeof(*node));
#endif
    RedisModule_Free(node);
}

static void new_detached_head(RedisModuleCtx *ctx, SelvaHierarchy *hierarchy, const Selva_NodeId node_id, Selva_NodeId *parents, size_t nr_parents) {
    const int prevIsDecompressingSubtree = isDecompressingSubtree;
    struct SelvaHierarchyNode *node;
    int err;

    /*
     * We are not actually decompressing but we need to make it look like we are.
     */
    isDecompressingSubtree = 1;
    err = SelvaHierarchy_UpsertNode(ctx, hierarchy, node_id, &node);
    isDecompressingSubtree = prevIsDecompressingSubtree;

    /* TODO what to do if there was an error? */
    if (!err) {
        /* TODO catch error */
        (void)SelvaModify_AddHierarchyP(ctx, hierarchy, node, nr_parents, parents, 0, NULL);

        node->flags.detached = 1;
        SelvaObject_Destroy(node->obj);
        node->obj = NULL;
    }
}

/**
 * Reinit everything that was removed when the subtree head was made detached.
 * There should be no need to ever call this function from anywhere else but
 * SelvaHierarchy_FindNode().
 */
static int repopulate_detached_head(SelvaHierarchyNode *node) {
    int err;

    err = create_node_object(node);
    if (err) {
        fprintf(stderr, "%s:%d: Failed to repopulate a detached dummy node %.*s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, node->id);
        return err;
    }

    node->flags.detached = 0;

    return 0;
}

SelvaHierarchyNode *SelvaHierarchy_FindNode(SelvaHierarchy *hierarchy, const Selva_NodeId id) {
    SelvaHierarchySearchFilter filter;
    SelvaHierarchyNode *node;
    int err;

    memcpy(&filter.id, id, SELVA_NODE_ID_SIZE);

    SELVA_TRACE_BEGIN(find_inmem);
    node = RB_FIND(hierarchy_index_tree, &hierarchy->index_head, (SelvaHierarchyNode *)(&filter));
    SELVA_TRACE_END(find_inmem);
    if (node) {
        if (!node->flags.detached) {
            return node;
        } else if (isDecompressingSubtree) {
            err = repopulate_detached_head(node);
            if (err) {
                return NULL;
            }

            return node;
        }
    }
    node = NULL; /* Make sure we don't use this accidentally. */

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
                fprintf(stderr, "%s:%d: Restoring a subtree containing %.*s failed: %s\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, id,
                        getSelvaErrorStr(err));
            }

            return NULL;
        }

        return RB_FIND(hierarchy_index_tree, &hierarchy->index_head, (SelvaHierarchyNode *)(&filter));
    }

    return NULL;
}

struct SelvaObject *SelvaHierarchy_GetNodeObject(const struct SelvaHierarchyNode *node) {
    return node->obj;
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
    if (!node) {
        return NULL;
    }

    return &node->metadata;
}

static const char * const excluded_fields[] = {
    SELVA_ID_FIELD,
    SELVA_TYPE_FIELD,
    SELVA_CREATED_AT_FIELD,
    SELVA_ALIASES_FIELD,
    NULL
};

int SelvaHierarchy_ClearNodeFields(struct SelvaObject *obj) {
    SelvaObject_Clear(obj, excluded_fields);

    return 0;
}

static inline void mkHead(SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    (void)SVector_InsertFast(&hierarchy->heads, node);
}

static inline void rmHead(SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    SVector_Remove(&hierarchy->heads, node);
}

/**
 * Delete all aliases from the aliases key.
 * Note that this function doesn't delete the aliases from the node object.
 */
static void delete_node_aliases(RedisModuleCtx *ctx, struct SelvaObject *obj) {
    struct SelvaSet *node_aliases_set;

    node_aliases_set = SelvaObject_GetSetStr(obj, SELVA_ALIASES_FIELD, sizeof(SELVA_ALIASES_FIELD) - 1);
    if (node_aliases_set) {
        RedisModuleKey *aliases_key;

        aliases_key = open_aliases_key(ctx);
        if (aliases_key) {
            delete_aliases(aliases_key, node_aliases_set);
            RedisModule_CloseKey(aliases_key);
        } else {
            fprintf(stderr, "%s: Unable to open aliases\n", __FILE__);
        }
    }
}

static void del_node(RedisModuleCtx *ctx, SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
    struct SelvaObject *obj = node->obj;
    Selva_NodeId id;
    int is_root;

    memcpy(id, node->id, SELVA_NODE_ID_SIZE);
    is_root = !memcmp(id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE);

    if (likely(ctx)) {
        Selva_Subscriptions_DeferTriggerEvents(ctx, hierarchy, node, SELVA_SUBSCRIPTION_TRIGGER_TYPE_DELETED);
    }

    removeRelationships(ctx, hierarchy, node, RELATIONSHIP_PARENT);
    SelvaSubscriptions_DeferHierarchyDeletionEvents(ctx, hierarchy, node);

    if (likely(ctx)) {
        delete_node_aliases(ctx, obj);
    }

    /*
     * Never delete the root node.
     */
    if (is_root) {
        SelvaHierarchy_ClearNodeFields(obj);
    } else {
        removeRelationships(ctx, hierarchy, node, RELATIONSHIP_CHILD);

        /*
         * The node was now marked as a head but we are going to get rid of it
         * soon, so there is no reason to make it a tree head. In fact, doing
         * so would break things.
         */
        rmHead(hierarchy, node);

        RB_REMOVE(hierarchy_index_tree, &hierarchy->index_head, node);
        SelvaModify_DestroyNode(ctx, hierarchy, node);
        SelvaObject_Destroy(obj);
    }
}

#if HIERARCHY_SORT_BY_DEPTH
static void updateDepth(SelvaHierarchy *hierarchy, SelvaHierarchyNode *head) {
    SVector q;

    if (unlikely(isRdbLoading(NULL))) {
        /*
         * Skip updates for now as it would require a full BFS pass for every new node.
         */
        return;
    }

    if (unlikely(!SVector_Init(&q, selva_glob_config.hierarchy_expected_resp_len, NULL))) {
        fprintf(stderr, "%s:%d: Depth update error\n", __FILE__, __LINE__);
        abort();
    }

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
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const SelvaHierarchyNode *node) {
    if (ctx && !isRdbLoading(ctx)) {
        SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, node, "ancestors", 9);
    }
}

static inline void publishDescendantsUpdate(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const SelvaHierarchyNode *node) {
    if (ctx && !isRdbLoading(ctx)) {
        SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, node, "descendants", 11);
    }
}

static inline void publishChildrenUpdate(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const SelvaHierarchyNode *node) {
    if (ctx && !isRdbLoading(ctx)) {
        SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, node, "children", 8);
    }
}

static inline void publishParentsUpdate(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const SelvaHierarchyNode *node) {
    if (ctx && !isRdbLoading(ctx)) {
        SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, node, "parents", 7);
    }
}

static int cross_insert_children(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        size_t n,
        const Selva_NodeId *nodes) {
    int res = 0;

    if (n == 0) {
        return 0; /* No changes. */
    }

    for (size_t i = 0; i < n; i++) {
        SelvaHierarchyNode *child;

        /* TODO Could we upsert here? */
        child = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
        if (!child) {
            int err;

            err = SelvaModify_SetHierarchy(ctx, hierarchy, nodes[i],
                    0, NULL,
                    0, NULL);
            if (err < 0) {
                fprintf(stderr, "%s:%d: Failed to create a child \"%.*s\" for \"%.*s\": %s\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, nodes[i],
                        (int)SELVA_NODE_ID_SIZE, node->id,
                        selvaStrError[-err]);
                continue;
            }

            child = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
            if (!child) {
                fprintf(stderr, "%s:%d: Node state error, node: \"%.*s\"\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, nodes[i]);
                return SELVA_HIERARCHY_EGENERAL;
            }
        }

        /* The child node is no longer an orphan */
        if (SVector_Size(&child->parents) == 0) {
            rmHead(hierarchy, child);
        }

        if (SVector_InsertFast(&node->children, child) == NULL) {
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
                ctx, hierarchy,
                child->id, &child->metadata,
                SVector_Size(&child->children),
                node);

            /*
             * Inherit markers from the new child to the parent node.
             */
            SelvaSubscriptions_InheritChild(
                ctx, hierarchy,
                node->id, &node->metadata,
                SVector_Size(&node->parents),
                child);

            /*
             * Publish that the parents field was changed.
             * Actual events are only sent if there are subscription markers
             * set on this node.
             */
            publishParentsUpdate(ctx, hierarchy, child);
            publishAncestorsUpdate(ctx, hierarchy, child);

            SelvaSubscriptions_DeferHierarchyEvents(ctx, hierarchy, child);

            res++; /* Count actual insertions */
        }

        publishChildrenUpdate(ctx, hierarchy, node);
        publishDescendantsUpdate(ctx, hierarchy, node);
    }

    SelvaSubscriptions_DeferHierarchyEvents(ctx, hierarchy, node);

    /*
     * Publish that the children field was changed.
     */
    if (res > 0) {
        publishChildrenUpdate(ctx, hierarchy, node);
        publishDescendantsUpdate(ctx, hierarchy, node);
#if HIERARCHY_SORT_BY_DEPTH
        updateDepth(hierarchy, node);
#endif
    }

    return res;
}

static int cross_insert_parents(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        size_t n,
        const Selva_NodeId *nodes) {
    int res = 0;

    if (n == 0) {
        return 0; /* No changes. */
    }

    if (SVector_Size(&node->parents) == 0) {
        /* The node is no longer an orphan */
        rmHead(hierarchy, node);
    }

    for (size_t i = 0; i < n; i++) {
        SelvaHierarchyNode *parent;

        parent = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
        if (!parent) {
            int err;

            /* RFE no_root is not propagated */
            err = SelvaModify_SetHierarchy(ctx, hierarchy, nodes[i],
                    1, ((Selva_NodeId []){ ROOT_NODE_ID }),
                    0, NULL);
            if (err < 0) {
                fprintf(stderr, "%s:%d: Failed to create a parent \"%.*s\" for \"%.*s\": %s\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, nodes[i],
                        (int)SELVA_NODE_ID_SIZE, node->id,
                        selvaStrError[-err]);
                continue;
            }

            parent = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
            if (!parent) {
                fprintf(stderr, "%s:%d: Node state error, node: \"%.*s\"\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, nodes[i]);
                return SELVA_HIERARCHY_EGENERAL;
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
                ctx, hierarchy,
                node->id, &node->metadata,
                SVector_Size(&node->children),
                parent);

            /*
             * Inherit subscription markers from the node to the new parent.
             */
            SelvaSubscriptions_InheritChild(
                ctx, hierarchy,
                parent->id, &parent->metadata,
                SVector_Size(&parent->parents),
                node);

            /*
             * Publish that the children field was changed.
             * Actual events are only sent if there are subscription markers
             * set on this node.
             */
            publishChildrenUpdate(ctx, hierarchy, parent);
            publishDescendantsUpdate(ctx, hierarchy, parent);

            SelvaSubscriptions_DeferHierarchyEvents(ctx, hierarchy, parent);

            res++;
        }
    }

    SelvaSubscriptions_DeferHierarchyEvents(ctx, hierarchy, node);

    /*
     * Publish that the parents field was changed.
     */
    if (res > 0) {
        publishParentsUpdate(ctx, hierarchy, node);
        publishAncestorsUpdate(ctx, hierarchy, node);
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
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        enum SelvaHierarchyNode_Relationship rel,
        size_t n,
        const Selva_NodeId *nodes,
        int pointers) {
    SVECTOR_AUTOFREE(sub_markers);

    /*
     * Backup the subscription markers so we can refresh them after the
     * operation.
     */
#ifndef PU_TEST_BUILD
    if (unlikely(!SVector_Clone(&sub_markers, &node->metadata.sub_markers.vec, NULL))) {
        return SELVA_HIERARCHY_ENOMEM;
    }
    SelvaSubscriptions_ClearAllMarkers(ctx, hierarchy, node);
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
            publishChildrenUpdate(ctx, hierarchy, parent);
            pubParents = 1;
        }

        if (initialNodeParentsSize > 0 && SVector_Size(&node->parents) == 0) {
            /* node is an orphan now */
            mkHead(hierarchy, node);
        }

        if (pubParents) {
            publishParentsUpdate(ctx, hierarchy, node);
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
            publishParentsUpdate(ctx, hierarchy, child);
            pubChildren = 1;
        }

        if (pubChildren) {
            publishChildrenUpdate(ctx, hierarchy, node);
        }
    } else {
        return SELVA_HIERARCHY_ENOTSUP;
    }

#if HIERARCHY_SORT_BY_DEPTH
    updateDepth(hierarchy, node);
#endif
    SelvaSubscriptions_RefreshByMarker(ctx, hierarchy, &sub_markers);

    return 0;
}

/**
 * Remove all relationships rel of node.
 */
static void removeRelationships(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        enum SelvaHierarchyNode_Relationship rel) {
    SVector *vec_a;
    size_t offset_a;
    size_t offset_b;
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
        assert(("rel is invalid", 0));
        return;
    }

    vec_a = (SVector *)((char *)node + offset_a);
    if (SVector_Size(vec_a) == 0) {
        return;
    }

    /*
     * Backup the subscription markers so we can refresh them after the
     * operation.
     */
#ifndef PU_TEST_BUILD
    if (unlikely(!SVector_Clone(&sub_markers, &node->metadata.sub_markers.vec, NULL))) {
        fprintf(stderr, "%s:%d: ENOMEM\n", __FILE__, __LINE__);
        return;
    }
#endif

    SelvaSubscriptions_ClearAllMarkers(ctx, hierarchy, node);

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

    SelvaSubscriptions_RefreshByMarker(ctx, hierarchy, &sub_markers);

    /*
     * After this the caller should call mkHead(hierarchy, node)
     * if rel == RELATIONSHIP_CHILD.
     */
}

void SelvaHierarchy_DelChildren(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node) {
    removeRelationships(ctx, hierarchy, node, RELATIONSHIP_PARENT);
}

void SelvaHierarchy_DelParents(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node) {
    removeRelationships(ctx, hierarchy, node, RELATIONSHIP_CHILD);
}

int SelvaModify_SetHierarchy(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaHierarchyNode *node;
    int isNewNode = 0;
    int err, res = 0;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        node = newNode(ctx, id);
        if (unlikely(!node)) {
            return SELVA_HIERARCHY_ENOMEM;
        }

        SelvaSubscriptions_DeferMissingAccessorEvents(hierarchy, id, SELVA_NODE_ID_SIZE);
        isNewNode = 1;
    }

    if (isNewNode) {
        if (unlikely(RB_INSERT(hierarchy_index_tree, &hierarchy->index_head, node) != NULL)) {
            SelvaModify_DestroyNode(ctx, hierarchy, node);

            return SELVA_HIERARCHY_EEXIST;
        }
        res++;
    } else {
        /* Clear the existing node relationships */
        removeRelationships(ctx, hierarchy, node, RELATIONSHIP_PARENT);
        removeRelationships(ctx, hierarchy, node, RELATIONSHIP_CHILD);
    }

    if (nr_parents == 0) {
        /* This node is orphan */
        mkHead(hierarchy, node);
    }

    /*
     * Set relationship relative to other nodes
     * RFE if isNewNode == 0 then errors are not handled properly as
     * we don't know how to rollback.
     */
    err = cross_insert_parents(ctx, hierarchy, node, nr_parents, parents);
    if (err < 0) {
        if (isNewNode) {
            del_node(ctx, hierarchy, node);
        }
        return err;
    }
    res += err;

    /* Same for the children */
    err = cross_insert_children(ctx, hierarchy, node, nr_children, children);
    if (err < 0) {
        if (isNewNode) {
            del_node(ctx, hierarchy, node);
        }
        return err;
    }
    res += err;

    return res;
}

/**
 * Remove adjacents not on the nodes list.
 */
static int remove_missing(
        RedisModuleCtx *ctx,
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
        fprintf(stderr, "%s:%d: ENOMEM\n", __FILE__, __LINE__);

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
                    rel == RELATIONSHIP_CHILD ? "parents" : "children",
                    (int)SELVA_NODE_ID_SIZE, adj->id);
#endif

            memcpy(arr, &adj, sizeof(SelvaHierarchyNode *));
            crossRemove(ctx, hierarchy, node, rel, 1, arr, 1);
            res++;
        }
    }

    return res;
}

int SelvaModify_SetHierarchyParents(
        RedisModuleCtx *ctx,
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
        removeRelationships(ctx, hierarchy, node, RELATIONSHIP_CHILD);

        /* This node is orphan. */
        mkHead(hierarchy, node);

        return 1; /* I guess we could deleting all as a single change. */
    }

    /*
     * Set relationship relative to other nodes.
     */
    err = cross_insert_parents(ctx, hierarchy, node, nr_parents, parents);
    if (err < 0) {
        return err;
    }
    res += err;

    /*
     * Remove parents that are not in the given list.
     */
    err = remove_missing(ctx, hierarchy, node, nr_parents, parents, RELATIONSHIP_CHILD);
    if (err < 0) {
        return err;
    }
    res += err;

    return res;
}

int SelvaModify_SetHierarchyChildren(
        RedisModuleCtx *ctx,
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
        removeRelationships(ctx, hierarchy, node, RELATIONSHIP_PARENT);

        return 1;
    }

    /*
     * Set relationship relative to other nodes.
     */
    err = cross_insert_children(ctx, hierarchy, node, nr_children, children);
    if (err < 0) {
        return err;
    }
    res += err;

    /*
     * Remove children that are not in the given list.
     */
    err = remove_missing(ctx, hierarchy, node, nr_children, children, RELATIONSHIP_PARENT);
    if (err < 0) {
        return err;
    }
    res += err;

    return res;
}

int SelvaHierarchy_UpsertNode(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        SelvaHierarchyNode **out) {
    SelvaHierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, id);
    SelvaHierarchyNode *prev_node;
    const int isLoading = isRdbLoading(ctx);

    if (node) {
        if (out) {
            *out = node;
        }

        return SELVA_HIERARCHY_EEXIST;
    }

    /*
     * newNode skips some tasks if ctx is set as NULL and it should be set NULL
     * when loading.
     */
     node = newNode(isLoading ? NULL : ctx, id);
     if (unlikely(!node)) {
         return SELVA_HIERARCHY_ENOMEM;
     }

     /*
      * No need to check if we have event registrations while loading the
      * database.
      */
     if (!isLoading) {
        SelvaSubscriptions_DeferMissingAccessorEvents(hierarchy, id, SELVA_NODE_ID_SIZE);
     }

     /*
      * All nodes must be indexed.
      */
     prev_node = RB_INSERT(hierarchy_index_tree, &hierarchy->index_head, node);
     if (prev_node) {
         /*
          * We are being extremely paranoid here as this shouldn't be possible.
          */
         SelvaModify_DestroyNode(ctx, hierarchy, node);

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
        RedisModuleCtx *ctx,
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
    err = cross_insert_parents(ctx, hierarchy, node, nr_parents, parents);
    if (err < 0) {
        return err;
    }
    res += err;

    /* Same for the children */
    err = cross_insert_children(ctx, hierarchy, node, nr_children, children);
    if (err < 0) {
        return err;
    }
    res += err;

    return res;
}

int SelvaModify_AddHierarchy(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaHierarchyNode *node;
    int isNewNode;
    int err;

    err = SelvaHierarchy_UpsertNode(ctx, hierarchy, id, &node);
    if (err == SELVA_HIERARCHY_EEXIST) {
        isNewNode = 0;
    } else if (err) {
        return err;
    } else {
        isNewNode = 1;
    }

    err = SelvaModify_AddHierarchyP(ctx, hierarchy, node, nr_parents, parents, nr_children, children);
    if (err < 0) {
        if (isNewNode) {
            del_node(ctx, hierarchy, node);
        }

        return err;
    }

    return err + isNewNode; /* Return the number of changes. */
}

int SelvaModify_DelHierarchy(
        RedisModuleCtx *ctx,
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

    err1 = crossRemove(ctx, hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents, 0);
    err2 = crossRemove(ctx, hierarchy, node, RELATIONSHIP_PARENT, nr_children, children, 0);

    return err1 ? err1 : err2;
}

static void copy_nodeIds(Selva_NodeId *dst, const struct SVector *vec) {
    struct SVectorIterator it;
    const SelvaHierarchyNode *node;

    SVector_ForeachBegin(&it, vec);
    while ((node = SVector_Foreach(&it))) {
        memcpy(dst++, node->id, SELVA_NODE_ID_SIZE);
    }
}

static int SelvaModify_DelHierarchyNodeP(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *node,
        enum SelvaModify_DelHierarchyNodeFlag flags,
        void *opt_arg) {
    size_t nr_ids;

    assert(("hierarchy must be set", hierarchy));
    assert(("node must be set", node));

    if (flags & DEL_HIERARCHY_NODE_DETACH) {
        struct compressed_rms *compressed = (struct compressed_rms *)opt_arg;

        if (!compressed) {
            return SELVA_HIERARCHY_EINVAL;
        }

        /* Add to the detached nodes. */
        SelvaHierarchyDetached_AddNode(hierarchy, node->id, compressed);
    }

    SelvaSubscriptions_ClearAllMarkers(ctx, hierarchy, node);

    nr_ids = SVector_Size(&node->children);
    if (nr_ids > 0) {
        Selva_NodeId *ids;

        ids = RedisModule_PoolAlloc(ctx, nr_ids * SELVA_NODE_ID_SIZE);
        if (!ids) {
            return SELVA_HIERARCHY_ENOMEM;
        }

        copy_nodeIds(ids, &node->children);

        /*
         * Delete orphan children recursively.
         */
        for (size_t i = 0; i < nr_ids; i++) {
            Selva_NodeId nodeId;
            int err;

            memcpy(nodeId, ids + i, SELVA_NODE_ID_SIZE);

            /*
             * Find the node.
             */
            SelvaHierarchyNode *child = SelvaHierarchy_FindNode(hierarchy, nodeId);
            if (!child) {
                /* Node not found;
                 * This is probably fine, as there might have been a circular link.
                 */
                continue;
            }

            /*
             * Note that we store a pointer in a Selva_NodeId array to save in
             * pointless RB_FIND() lookups.
             */
            Selva_NodeId arr[1];
            memcpy(arr, &child, sizeof(SelvaHierarchyNode *));
            err = crossRemove(ctx, hierarchy, node, RELATIONSHIP_PARENT, 1, arr, 1);
            if (err) {
                return err;
            }

            /*
             * Recursively delete the child and its children if its parents field is
             * empty and no edge fields are pointing to it.
             */
            if ((flags & DEL_HIERARCHY_NODE_FORCE) || (SVector_Size(&child->parents) == 0 && Edge_Refcount(child) == 0)) {
                err = SelvaModify_DelHierarchyNodeP(ctx, hierarchy, child, flags, opt_arg);
                if (err) {
                    return err;
                }
            }
        }
    }

    del_node(ctx, hierarchy, node);

    return 0;
}

int SelvaModify_DelHierarchyNode(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaModify_DelHierarchyNodeFlag flags) {
    SelvaHierarchyNode *node;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return SelvaModify_DelHierarchyNodeP(ctx, hierarchy, node, flags, NULL);
}

static int SelvaHierarchyHeadCallback_Dummy(SelvaHierarchyNode *node, void *arg) {
    REDISMODULE_NOT_USED(node);
    REDISMODULE_NOT_USED(arg);

    return 0;
}

static int HierarchyNode_Callback_Dummy(SelvaHierarchyNode *node, void *arg) {
    REDISMODULE_NOT_USED(node);
    REDISMODULE_NOT_USED(arg);

    return 0;
}

static void SelvaHierarchyChildCallback_Dummy(const struct SelvaHierarchyTraversalMetadata *metadata, SelvaHierarchyNode *child, void *arg) {
    REDISMODULE_NOT_USED(metadata);
    REDISMODULE_NOT_USED(child);
    REDISMODULE_NOT_USED(arg);
}

/**
 * DFS from a given head node towards its descendants or ancestors.
 */
static int dfs(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *head,
        enum SelvaHierarchyNode_Relationship dir,
        const struct SelvaHierarchyCallback * restrict cb) {
    SelvaHierarchyHeadCallback head_cb = cb->head_cb ? cb->head_cb : &SelvaHierarchyHeadCallback_Dummy;
    SelvaHierarchyNodeCallback node_cb = cb->node_cb ? cb->node_cb : &HierarchyNode_Callback_Dummy;
    SelvaHierarchyChildCallback child_cb = cb->child_cb ? cb->child_cb : &SelvaHierarchyChildCallback_Dummy;
    size_t offset;
    struct SelvaHierarchyTraversalMetadata child_metadata;

    switch (dir) {
    case RELATIONSHIP_PARENT:
        child_metadata.origin_field_str = (const char *)"parents";
        child_metadata.origin_field_len = 7;
        offset = offsetof(SelvaHierarchyNode, parents);
        break;
    case RELATIONSHIP_CHILD:
        child_metadata.origin_field_str = (const char *)"children";
        child_metadata.origin_field_len = 8;
        offset = offsetof(SelvaHierarchyNode, children);
        break;
    default:
        return SELVA_HIERARCHY_ENOTSUP;
    }

    SVECTOR_AUTOFREE(stack);
    if (unlikely(!SVector_Init(&stack, selva_glob_config.hierarchy_expected_resp_len, NULL))) {
        return SELVA_HIERARCHY_ENOMEM;
    }

    int err = 0;
    struct trx trx_cur;
    if (Trx_Begin(&hierarchy->trx_state, &trx_cur)) {
        return SELVA_HIERARCHY_ETRMAX;
    }

    SVector_Insert(&stack, head);
    if (head_cb(head, cb->head_arg)) {
        err = 0;
        goto out;
    }

    while (SVector_Size(&stack) > 0) {
        SelvaHierarchyNode *node;

        node = SVector_Pop(&stack);
        if (Trx_Visit(&trx_cur, &node->trx_label)) {
            if (node_cb(node, cb->node_arg)) {
                err = 0;
                goto out;
            }

            /* Add parents/children of this node to the stack of unvisited nodes */
            struct SVectorIterator it;
            SelvaHierarchyNode *adj;
            const SVector *vec = (SVector *)((char *)node + offset);

            SVector_ForeachBegin(&it, vec);
            while ((adj = SVector_Foreach(&it))) {
                if (adj->flags.detached) {
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
                child_cb(&child_metadata, adj, cb->child_arg);

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
static int full_dfs(SelvaHierarchy *hierarchy, const struct SelvaHierarchyCallback * restrict cb) {
    SelvaHierarchyHeadCallback head_cb = cb->head_cb ? cb->head_cb : &SelvaHierarchyHeadCallback_Dummy;
    SelvaHierarchyNodeCallback node_cb = cb->node_cb ? cb->node_cb : &HierarchyNode_Callback_Dummy;
    SelvaHierarchyChildCallback child_cb = cb->child_cb ? cb->child_cb : &SelvaHierarchyChildCallback_Dummy;
    SelvaHierarchyNode *head;
    SVECTOR_AUTOFREE(stack);

    if (unlikely(!SVector_Init(&stack, selva_glob_config.hierarchy_expected_resp_len, NULL))) {
        return SELVA_HIERARCHY_ENOMEM;
    }

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

    SVector_ForeachBegin(&it, &hierarchy->heads);
    while ((head = SVector_Foreach(&it))) {
        SVector_Insert(&stack, head);

        if (head->flags.detached) {
            err = restore_subtree(hierarchy, head->id);
            if (err) {
                /*
                 * The error is already logged,
                 * we just try to bail from here.
                 */
                goto out;
            }
        }

        if (head_cb(head, cb->head_arg)) {
            err = 0;
            goto out;
        }

        while (SVector_Size(&stack) > 0) {
            SelvaHierarchyNode *node = SVector_Pop(&stack);

            if (Trx_Visit(&trx_cur, &node->trx_label)) {
                if (node_cb(node, cb->node_arg)) {
                    err = 0;
                    goto out;
                }

                struct SVectorIterator it2;
                SelvaHierarchyNode *adj;

                SVector_ForeachBegin(&it2, &node->children);
                while ((adj = SVector_Foreach(&it2))) {
                    if (adj->flags.detached) {
                        err = restore_subtree(hierarchy, adj->id);
                        if (err) {
                            goto out;
                        }
                    }

                    child_metadata.origin_node = node; /* parent */
                    child_cb(&child_metadata, adj, cb->child_arg);

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

#define BFS_TRAVERSE(hierarchy, head, cb) \
    SelvaHierarchyHeadCallback head_cb = (cb)->head_cb ? (cb)->head_cb : SelvaHierarchyHeadCallback_Dummy; \
    SelvaHierarchyNodeCallback node_cb = (cb)->node_cb ? (cb)->node_cb : HierarchyNode_Callback_Dummy; \
    SelvaHierarchyChildCallback child_cb = (cb)->child_cb ? (cb)->child_cb : SelvaHierarchyChildCallback_Dummy; \
    \
    SVECTOR_AUTOFREE(_bfs_q); \
    if (unlikely(!SVector_Init(&_bfs_q, selva_glob_config.hierarchy_expected_resp_len, NULL))) { \
        return SELVA_HIERARCHY_ENOMEM; \
    } \
    \
    struct trx trx_cur; \
    if (Trx_Begin(&(hierarchy)->trx_state, &trx_cur)) { \
        return SELVA_HIERARCHY_ETRMAX; \
    } \
    \
    Trx_Visit(&trx_cur, &(head)->trx_label); \
    SVector_Insert(&_bfs_q, (head)); \
    if (head_cb((head), (cb)->head_arg)) { Trx_End(&hierarchy->trx_state, &trx_cur); return 0; } \
    while (SVector_Size(&_bfs_q) > 0) { \
        SelvaHierarchyNode *node = SVector_Shift(&_bfs_q);

#define BFS_VISIT_NODE() \
        if (node_cb(node, cb->node_arg)) { \
            Trx_End(&hierarchy->trx_state, &trx_cur); \
            return 0; \
        }

#define BFS_VISIT_ADJACENT(_origin_field_str, _origin_field_len, adj_node) do { \
        if (Trx_Visit(&trx_cur, &(adj_node)->trx_label)) { \
            if ((adj_node)->flags.detached) { \
                int subtree_err = restore_subtree(hierarchy, (adj_node)->id); \
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
            child_cb(&_cb_metadata, (adj_node), cb->child_arg); \
            SVector_Insert(&_bfs_q, (adj_node)); \
        } \
    } while (0)

#define BFS_VISIT_ADJACENTS(origin_field_str, origin_field_len, adj_vec) do { \
        struct SVectorIterator _bfs_visit_it; \
        \
        SVector_ForeachBegin(&_bfs_visit_it, (adj_vec)); \
        SelvaHierarchyNode *_adj; \
        while ((_adj = SVector_Foreach(&_bfs_visit_it))) { \
            BFS_VISIT_ADJACENT((origin_field_str), (origin_field_len), _adj); \
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
    if (field_len == 8 && !strncmp("children", field_str, 8)) {
        *field_type = SELVA_HIERARCHY_TRAVERSAL_CHILDREN;
        return &node->children;
    } else if (field_len == 7 && !strncmp("parents", field_str, 7)) {
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
}

/**
 * Execute an edge filter for the node.
 * @param ctx is a pointer to the Redis context.
 * @param edge_filter_ctx is a context for the filter.
 * @param edge_filter is a pointer to the compiled edge filter.
 * @param adj_vec is a pointer to the arcs vector field of an EdgeField structure.
 * @param node is a pointer to the node the edge is pointing to.
 */
static int exec_edge_filter(
        struct RedisModuleCtx *ctx,
        struct rpn_ctx *edge_filter_ctx,
        const struct rpn_expression *edge_filter,
        const SVector *adj_vec,
        const SelvaHierarchyNode *node) {
    struct EdgeField *edge_field = containerof(adj_vec, struct EdgeField, arcs);
    struct SelvaObject *edge_metadata;
    int err;

    err = Edge_GetFieldEdgeMetadata(edge_field, node->id, 0, &edge_metadata);
    if (err == SELVA_HIERARCHY_ENOENT || err == SELVA_ENOENT) {
#if 0
        fprintf(stderr, "No metadata for this edge: %.*s.%s -> %.*s\n",
                (int)SELVA_NODE_ID_SIZE, EMPTY_NODE_ID, //TODO head->id,
                "", // TODO RedisModule_StringPtrLen(field, NULL),
                (int)SELVA_NODE_ID_SIZE, node->id);
#endif
        /* TODO Should probably run the filter with an empty object. */
        return 0;
    } else if (err) {
        /* TODO Error handling */
#if 0
        fprintf(stderr, "%s:%d: Failed to get edge metadata %.*s.%s -> %.*s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, EMPTY_NODE_ID, // TODO head->id,
                "", // TODO RedisModule_StringPtrLen(field, NULL),
                (int)SELVA_NODE_ID_SIZE, node->id);
#endif
        return 0;
    } else {
        enum rpn_error rpn_err;
        int res;

        rpn_set_reg(edge_filter_ctx, 0, node->id, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
        rpn_set_hierarchy_node(edge_filter_ctx, node);
        rpn_set_obj(edge_filter_ctx, edge_metadata);

        rpn_err = rpn_bool(ctx, edge_filter_ctx, edge_filter, &res); /* TODO Handle RPN errors. */
        if (!rpn_err && res) {
            return 1;
        }
    }

    return 0;
}

/**
 * BFS from a given head node towards its descendants or ancestors.
 */
static __hot int bfs(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *head,
        enum SelvaHierarchyNode_Relationship dir,
        const struct SelvaHierarchyCallback * restrict cb) {
    const char *origin_field_str;
    size_t origin_field_len;
    size_t offset;

    switch (dir) {
    case RELATIONSHIP_PARENT:
        origin_field_str = (const char *)"parents";
        origin_field_len = 7;
        offset = offsetof(SelvaHierarchyNode, parents);
        break;
    case RELATIONSHIP_CHILD:
        origin_field_str = (const char *)"children";
        origin_field_len = 8;
        offset = offsetof(SelvaHierarchyNode, children);
        break;
    default:
        return SELVA_HIERARCHY_ENOTSUP;
    }

    BFS_TRAVERSE(hierarchy, head, cb) {
        const SVector *adj_vec = (SVector *)((char *)node + offset);

        BFS_VISIT_NODE();
        BFS_VISIT_ADJACENTS(origin_field_str, origin_field_len, adj_vec);
    } BFS_TRAVERSE_END(hierarchy);

    return 0;
}

static int bfs_edge(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *head,
        const char *field_name_str,
        size_t field_name_len,
        const struct SelvaHierarchyCallback * restrict cb) {
    BFS_TRAVERSE(hierarchy, head, cb) {
        const struct EdgeField *edge_field;

        BFS_VISIT_NODE();

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

        BFS_VISIT_ADJACENTS(field_name_str, field_name_len, &edge_field->arcs);
    } BFS_TRAVERSE_END(hierarchy);

    return 0;
}

static int bfs_expression(
        RedisModuleCtx *redis_ctx,
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *head,
        struct rpn_ctx *rpn_ctx,
        const struct rpn_expression *rpn_expr,
        struct rpn_ctx *edge_filter_ctx,
        const struct rpn_expression *edge_filter,
        const struct SelvaHierarchyCallback * restrict cb) {
    BFS_TRAVERSE(hierarchy, head, cb) {
        enum rpn_error rpn_err;
        struct SelvaSet fields;
        struct SelvaSetElement *field_el;

        SelvaSet_Init(&fields, SELVA_SET_TYPE_RMSTRING);

        rpn_set_reg(rpn_ctx, 0, node->id, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
        rpn_set_hierarchy_node(rpn_ctx, node);
        rpn_set_obj(rpn_ctx, SelvaHierarchy_GetNodeObject(node));
        rpn_err = rpn_selvaset(redis_ctx, rpn_ctx, rpn_expr, &fields);
        if (rpn_err) {
            fprintf(stderr, "%s:%d: RPN field selector expression failed for %.*s: %s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node->id,
                    rpn_str_error[rpn_err]);
            continue;
        }

        BFS_VISIT_NODE();

        SELVA_SET_RMS_FOREACH(field_el, &fields) {
            const RedisModuleString *field = field_el->value_rms;
            TO_STR(field);
            const SVector *adj_vec;
            enum SelvaTraversal field_type;
            struct SVectorIterator it;
            SelvaHierarchyNode *adj;

            adj_vec = get_adj_vec(node, field_str, field_len, &field_type);
            if (!adj_vec) {
                continue;
            }

            SVector_ForeachBegin(&it, adj_vec);
            while ((adj = SVector_Foreach(&it))) {
                /*
                 * If the field is an edge field we can filter edges by the
                 * edge metadata.
                 */
                if (field_type == SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD && edge_filter &&
                    !Trx_HasVisited(&trx_cur, &adj->trx_label) && /* skip if already visited. */
                    !exec_edge_filter(redis_ctx, edge_filter_ctx, edge_filter, adj_vec, adj)) {
                    continue;
                }

                BFS_VISIT_ADJACENT(field_str, field_len, adj);
            }
        }

        SelvaSet_Destroy(&fields);
    } BFS_TRAVERSE_END(hierarchy);

    return 0;
}

static void traverse_adjacents(
        const SVector *adj_vec,
        const struct SelvaHierarchyCallback *cb) {
    struct SVectorIterator it;

    if (cb->node_cb) {
        SelvaHierarchyNode *node;

        SVector_ForeachBegin(&it, adj_vec);
        while ((node = SVector_Foreach(&it))) {
            /* RFE Should we also call child_cb? */
            if (cb->node_cb(node, cb->node_arg)) {
                break;
            }
        }
    }
}

static void traverse_edge_field(
        SelvaHierarchyNode *head,
        const char *ref_field_str,
        size_t ref_field_len,
        const struct SelvaHierarchyCallback *cb) {
    const struct EdgeField *edge_field;

    if (cb->head_cb && cb->head_cb(head, cb->head_arg)) {
        return;
    }

    if (cb->node_cb) {
        edge_field = Edge_GetField(head, ref_field_str, ref_field_len);
        if (edge_field) {
            traverse_adjacents(&edge_field->arcs, cb);
        }
    }
}

static int traverse_ref(
        SelvaHierarchy *hierarchy,
        SelvaHierarchyNode *head,
        const char *ref_field_str,
        size_t ref_field_len,
        const struct SelvaHierarchyCallback *cb) {
    struct SelvaObject *head_obj = head->obj;
    struct SelvaSet *ref_set;

    if (cb->head_cb && cb->head_cb(head, cb->head_arg)) {
        return 0;
    }

    ref_set = SelvaObject_GetSetStr(head_obj, ref_field_str, ref_field_len);
    if (!ref_set) {
        return SELVA_HIERARCHY_ENOENT;
    }
    if (ref_set->type != SELVA_SET_TYPE_RMSTRING) {
        return SELVA_EINTYPE;
    }

    struct SelvaSetElement *el;
    SELVA_SET_RMS_FOREACH(el, ref_set) {
        Selva_NodeId nodeId;
        SelvaHierarchyNode *node;
        const RedisModuleString *value = el->value_rms;
        TO_STR(value);

        memset(nodeId, 0, SELVA_NODE_ID_SIZE);
        memcpy(nodeId, value_str, min(value_len, SELVA_NODE_ID_SIZE));

        node = SelvaHierarchy_FindNode(hierarchy, nodeId);
        if (node) {
            if (cb->node_cb(node, cb->node_arg)) {
                return 0;
            }
        }
    }

    return 0;
}

static int traverse_bfs_edge_field(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        const char *field_name_str,
        size_t field_name_len,
        const struct SelvaHierarchyCallback *cb) {
    SelvaHierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return bfs_edge(hierarchy, head, field_name_str, field_name_len, cb);
}

static int traverse_array(
        const SelvaHierarchyNode *head,
        const char *ref_field_str,
        size_t ref_field_len,
        const struct SelvaModify_ArrayObjectCallback *cb) {
    struct SelvaObject *head_obj = head->obj;
    int err;

    enum SelvaObjectType subtype;
    SVector *vec;
    err = SelvaObject_GetArrayStr(head_obj, ref_field_str, ref_field_len, &subtype, &vec);
    if (err) {
        return err;
    }

    struct SVectorIterator it;
    SVector_ForeachBegin(&it, vec);
    do {
        struct SelvaObject *obj;

        obj = SVector_Foreach(&it);
        if (obj && cb->node_cb(obj, cb->node_arg)) {
            return 0;
        }
    } while (!SVector_Done(&it));

    return 0;
}

int SelvaHierarchy_Traverse(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaTraversal dir,
        const struct SelvaHierarchyCallback *cb) {
    SelvaHierarchyNode *head;
    int err;

    if (dir == SELVA_HIERARCHY_TRAVERSAL_NONE) {
        return SELVA_HIERARCHY_EINVAL;
    }

    if (dir != SELVA_HIERARCHY_TRAVERSAL_DFS_FULL) {
        head = SelvaHierarchy_FindNode(hierarchy, id);
        if (!head) {
            return SELVA_HIERARCHY_ENOENT;
        }
    }

    switch (dir) {
    case SELVA_HIERARCHY_TRAVERSAL_NODE:
        cb->node_cb(head, cb->node_arg);
        err = 0;
        break;
    case SELVA_HIERARCHY_TRAVERSAL_CHILDREN:
        err = 0;
        if (cb->head_cb && cb->head_cb(head, cb->head_arg)) {
            break;
        }
        traverse_adjacents(&head->children, cb);
        break;
    case SELVA_HIERARCHY_TRAVERSAL_PARENTS:
        err = 0;
        if (cb->head_cb && cb->head_cb(head, cb->head_arg)) {
            break;
        }
        traverse_adjacents(&head->parents, cb);
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
        fprintf(stderr, "%s:%d: Invalid traversal requested (%d)\n",
                __FILE__, __LINE__,
                (int)dir);
        err = SELVA_HIERARCHY_ENOTSUP;
    }

    return err;
}

int SelvaHierarchy_TraverseField(
        SelvaHierarchy *hierarchy,
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

    switch (dir) {
    case SELVA_HIERARCHY_TRAVERSAL_REF:
        return traverse_ref(hierarchy, head, field_name_str, field_name_len, cb);
    case SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD:
        traverse_edge_field(head, field_name_str, field_name_len, cb);
        return 0;
    case SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD:
        return traverse_bfs_edge_field(hierarchy, id, field_name_str, field_name_len, cb);
     default:
        /* Should probably use some other traversal function. */
        fprintf(stderr, "%s:%d: Invalid traversal requested (%d)\n",
                __FILE__, __LINE__,
                (int)dir);
        return SELVA_HIERARCHY_ENOTSUP;
    }
}

int SelvaHierarchy_TraverseExpression(
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
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

    SelvaSet_Init(&fields, SELVA_SET_TYPE_RMSTRING);

    rpn_set_reg(rpn_ctx, 0, head->id, SELVA_NODE_ID_SIZE, RPN_SET_REG_FLAG_IS_NAN);
    rpn_set_hierarchy_node(rpn_ctx, head);
    rpn_set_obj(rpn_ctx, SelvaHierarchy_GetNodeObject(head));
    rpn_err = rpn_selvaset(ctx, rpn_ctx, rpn_expr, &fields);
    if (rpn_err) {
        fprintf(stderr, "%s:%d: RPN field selector expression failed for %.*s: %s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, head->id,
                rpn_str_error[rpn_err]);
        return SELVA_HIERARCHY_EINVAL;
    }

    /* For each field in the set. */
    SELVA_SET_RMS_FOREACH(field_el, &fields) {
        const RedisModuleString *field = field_el->value_rms;
        TO_STR(field);
        const SVector *adj_vec;
        struct SVectorIterator it;
        SelvaHierarchyNode *adj;
        enum SelvaTraversal field_type;

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
                !exec_edge_filter(ctx, edge_filter_ctx, edge_filter, adj_vec, adj)) {
                continue;
            }

            if (Trx_Visit(&trx_cur, &adj->trx_label)) {
                if (cb->node_cb(adj, cb->node_arg)) {
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
        struct RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
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

    return bfs_expression(ctx, hierarchy, head, rpn_ctx, rpn_expr, edge_filter_ctx, edge_filter, cb);
}

int SelvaModify_TraverseArray(
        SelvaHierarchy *hierarchy,
        const Selva_NodeId id,
        const char *ref_field_str,
        size_t ref_field_len,
        const struct SelvaModify_ArrayObjectCallback *cb) {
    const SelvaHierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return traverse_array(head, ref_field_str, ref_field_len, cb);
}

int SelvaHierarchy_IsNonEmptyField(const struct SelvaHierarchyNode *node, const char *field_str, size_t field_len) {
    if ((field_len == 7 && !strncmp("parents", field_str, 7)) ||
        (field_len == 9 && !strncmp("ancestors", field_str, 9))) {
        return SVector_Size(&node->parents) > 0;
    } else if ((field_len == 8 && !strncmp("children", field_str, 8)) ||
               (field_len == 11 && !strncmp("descendants", field_str, 11))) {
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
}

int load_metadata(RedisModuleIO *io, int encver, SelvaHierarchy *hierarchy, SelvaHierarchyNode *node) {
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
     * node->obj is currently NULL because it's not created when
     * isRdbLoading() is true.
     */
    node->obj = SelvaObjectTypeRDBLoad(io, encver, NULL);
    if (!node->obj) {
        return SELVA_ENOENT;
    }

    return 0;
}

static int load_tree(RedisModuleIO *io, int encver, SelvaHierarchy *hierarchy) {
    while (1) {
        int err;
        size_t len = 0;
        char *node_id __auto_free = NULL;

        node_id = RedisModule_LoadStringBuffer(io, &len);
        if (!node_id || len != SELVA_NODE_ID_SIZE) {
            RedisModule_LogIOError(io, "warning", "Failed to load the next nodeId");
            return SELVA_HIERARCHY_EINVAL;
        }

        /*
         * If it's EOF there are no more nodes for this hierarchy.
         */
        if (!memcmp(node_id, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE)) {
            break;
        }

        if (isDecompressingSubtree) {
            SelvaHierarchyDetached_RemoveNode(hierarchy, node_id);
        }

        /*
         * Upsert the node.
         */
        SelvaHierarchyNode *node;
        err = SelvaHierarchy_UpsertNode(RedisModule_GetContextFromIO(io), hierarchy, node_id, &node);
        if (err && err != SELVA_HIERARCHY_EEXIST) {
            RedisModule_LogIOError(io, "warning", "Failed to upsert %.*s: %s",
                                   (int)SELVA_NODE_ID_SIZE, node_id,
                                   getSelvaErrorStr(err));
            return err;
        }

        /*
         * The node metadata comes right after the node_id.
         */
        err = load_metadata(io, encver, hierarchy, node);
        if (err) {
            RedisModule_LogIOError(io, "warning", "Failed to load hierarchy metadata");
            return err;
        }

        /*
         * Load the ids of child nodes.
         */
        uint64_t nr_children = RedisModule_LoadUnsigned(io);
        Selva_NodeId *children __auto_free = NULL;

        if (nr_children > 0) {
            children = RedisModule_Alloc(nr_children * SELVA_NODE_ID_SIZE);
            if (!children) {
                return SELVA_HIERARCHY_ENOMEM;
            }

            /* Create/Update children */
            for (uint64_t i = 0; i < nr_children; i++) {
                char *child_id __auto_free = NULL;

                child_id = RedisModule_LoadStringBuffer(io, &len);
                if (len != SELVA_NODE_ID_SIZE) {
                    return SELVA_HIERARCHY_EINVAL;
                }

                if (isDecompressingSubtree) {
                    SelvaHierarchyDetached_RemoveNode(hierarchy, child_id);
                }

                err = SelvaModify_AddHierarchy(NULL, hierarchy, child_id, 0, NULL, 0, NULL);
                if (err < 0) {
                    RedisModule_LogIOError(io, "warning", "Unable to rebuild the hierarchy: %s",
                                           getSelvaErrorStr(err));
                    return err;
                }

                memcpy(children + i, child_id, SELVA_NODE_ID_SIZE);
            }
        }

        /*
         * Insert children of the node.
         */
        err = SelvaModify_AddHierarchyP(NULL, hierarchy, node, 0, NULL, nr_children, children);
        if (err < 0) {
            RedisModule_LogIOError(io, "warning", "Unable to rebuild the hierarchy: %s",
                                   getSelvaErrorStr(err));
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

static void *Hierarchy_RDBLoad(RedisModuleIO *io, int encver) {
    SelvaHierarchy *hierarchy;
    int err;

    if (encver != HIERARCHY_ENCODING_VERSION) {
        RedisModule_LogIOError(io, "warning", "selva_hierarchy encoding version %d not supported", encver);
        return NULL;
    }

    hierarchy = SelvaModify_NewHierarchy(RedisModule_GetContextFromIO(io));
    if (!hierarchy) {
        RedisModule_LogIOError(io, "warning", "Failed to create a new hierarchy");
        return NULL;
    }

    err = EdgeConstraint_RdbLoad(io, encver, &hierarchy->edge_field_constraints);
    if (err) {
        RedisModule_LogIOError(io, "warning", "Failed to load the dynamic constraints: %s", getSelvaErrorStr(err));
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

static void save_metadata(RedisModuleIO *io, SelvaHierarchyNode *node) {
    /*
     * Note that the metadata must be loaded and saved in predefined order.
     */

    Edge_RdbSave(io, node);
    SelvaObjectTypeRDBSave(io, node->obj, NULL);
}

static int HierarchyRDBSaveNode(SelvaHierarchyNode *node, void *arg) {
    RedisModuleIO *io = (RedisModuleIO *)arg;

    RedisModule_SaveStringBuffer(io, node->id, SELVA_NODE_ID_SIZE);
    save_metadata(io, node);
    RedisModule_SaveUnsigned(io, SVector_Size(&node->children));

    return 0;
}

static void HierarchyRDBSaveChild(const struct SelvaHierarchyTraversalMetadata *metadata, SelvaHierarchyNode *child, void *arg) {
    REDISMODULE_NOT_USED(metadata);
    RedisModuleIO *io = (RedisModuleIO *)arg;

    RedisModule_SaveStringBuffer(io, child->id, SELVA_NODE_ID_SIZE);
}

static void Hierarchy_RDBSave(RedisModuleIO *io, void *value) {
    SelvaHierarchy *hierarchy = (SelvaHierarchy *)value;
    const struct SelvaHierarchyCallback cb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = HierarchyRDBSaveNode,
        .node_arg = io,
        .child_cb = HierarchyRDBSaveChild,
        .child_arg = io,
    };

    /*
     * Serialization format:
     * EDGE_CONSTRAINTS
     * NODE_ID1 | METADATA | NR_CHILDREN | CHILD_ID_0,..
     * NODE_ID2 | METADATA | NR_CHILDREN | ...
     * HIERARCHY_RDB_EOF
     */
    EdgeConstraint_RdbSave(io, &hierarchy->edge_field_constraints);
    (void)full_dfs(hierarchy, &cb);
    RedisModule_SaveStringBuffer(io, HIERARCHY_RDB_EOF, sizeof(HIERARCHY_RDB_EOF));
}

int load_nodeId(RedisModuleIO *io, Selva_NodeId nodeId) {
    const char *buf __auto_free;
    size_t len;

    buf = RedisModule_LoadStringBuffer(io, &len);
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
static void *Hierarchy_SubtreeRDBLoad(RedisModuleIO *io, int encver) {
    SelvaHierarchy *hierarchy = subtree_hierarchy;
    int err;

    /*
     * TODO Currently Redis passes the encver always as 0. We might want to
     * patch a support for some sort of enc version passing, either just as a
     * long long inside the RDB string or some other way.
     */
#if 0
    if (encver != HIERARCHY_ENCODING_VERSION) {
        RedisModule_LogIOError(io, "warning", "selva_hierarchy encoding version %d not supported", encver);
        return NULL;
    }
#endif

    /*
     * 1. Read nodeId
     * 3. Load the children normally
     */

    Selva_NodeId nodeId;

    /*
     * Read nodeId.
     */
    if (load_nodeId(io, nodeId)) {
        return NULL;
    }

    SelvaHierarchyDetached_RemoveNode(hierarchy, nodeId);

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
static void Hierarchy_SubtreeRDBSave(RedisModuleIO *io, void *value) {
    struct SelvaHierarchySubtree *subtree = (struct SelvaHierarchySubtree *)value;
    SelvaHierarchy *hierarchy = subtree->hierarchy;
    struct SelvaHierarchyNode *node = subtree->node;
    const struct SelvaHierarchyCallback cb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = HierarchyRDBSaveNode,
        .node_arg = io,
        .child_cb = HierarchyRDBSaveChild,
        .child_arg = io,
    };

    /* Save nodeId. */
    RedisModule_SaveStringBuffer(io, node->id, SELVA_NODE_ID_SIZE);

    /*
     * Save the children.
     */
    (void)dfs(hierarchy, node, RELATIONSHIP_CHILD, &cb);
    RedisModule_SaveStringBuffer(io, HIERARCHY_RDB_EOF, sizeof(HIERARCHY_RDB_EOF));
}

/**
 * DO NOT CALL DIRECTLY. USE verifyDetachableSubtree().
 */
static int verifyDetachableSubtreeNodeCb(SelvaHierarchyNode *node, void *arg) {
    struct verifyDetachableSubtree *data = (struct verifyDetachableSubtree *)arg;

    /*
     * Currently we don't consider a subtree detachable if it uses any Edge features:
     * 1) any other node pointing to any node in the subtree using an edge field.
     * 2) any node in the subtree using edge fields.
     */
    if (Edge_Usage(node) != 0) {
        data->err = SELVA_HIERARCHY_ENOTSUP;
        return 1;
    }

    /*
     * Check that there are no active subscription markers on the node.
     * Subs starting from root can be ignored.
     */
    if (SelvaSubscriptions_hasActiveMarkers(&node->metadata)) {
        data->err = SELVA_HIERARCHY_ENOTSUP;
        return 1;
    }

    /*
     * Check that the parents of this node have been already traversed.
     * If this is not true then the we are not traversed a true subtree because
     * when doing a BFS the parents of every node (but the first node) should
     * have been visited before the node is visited.
     */
    struct SVectorIterator it;
    const SelvaHierarchyNode *parent;
    SVector_ForeachBegin(&it, &node->parents);
    while ((parent = SVector_Foreach(&it))) {
        if (!(parent == data->head || Trx_HasVisited(&data->trx_cur, &parent->trx_label))) {
            data->err = SELVA_HIERARCHY_ENOTSUP;
            return 1; /* not a true subtree. */
        }
    }

    return 0;
}

/**
 * Verify that the children of node can be safely detached.
 * Detachable subtree is subnetwork that the descendants of node can be safely
 * removed from the hierarchy, serialized and freed from memory. The subtree is
 * allowed be a acyclic network but `node` must be its true parent, i.e. the whole
 * subtree has only a single parent node that is `node`.
 * This function checks that the children of node form a true subtree that
 * doesn't cycle back to `node` and there are no active subscription markers or
 * other live dependencies on any of the nodes.
 * TODO Currently edge fields are not supported and thus if there are any edge
 * fields the check will return an error. Techincally a valid subtree could have
 * edge fields that only points to other nodes of the same subtree.
 * @return 0 is returned if the subtree is detachable;
 *         Otherwise a SelvaError is returned.
 */
static int verifyDetachableSubtree(SelvaHierarchy *hierarchy, struct SelvaHierarchyNode *node) {
    struct verifyDetachableSubtree data = {
        .err = 0,
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
    int err = 0;

    if (Trx_Begin(&hierarchy->trx_state, &data.trx_cur)) {
        return SELVA_HIERARCHY_ETRMAX;
    }

    struct SVectorIterator it;
    SelvaHierarchyNode *child;
    SVector_ForeachBegin(&it, &node->children);
    while ((child = SVector_Foreach(&it))) {
        err = bfs(hierarchy, child, RELATIONSHIP_CHILD, &cb);
        if (err) {
            break;
        } else if (data.err) {
            err = data.err;
            break;
        } else if (Trx_HasVisited(&data.trx_cur, &node->trx_label)) {
            /*
             * The traversal should not cycle back to the node if `node` is
             * really the head of this subtree.
             */
            err = SELVA_HIERARCHY_ENOTSUP;
            break;
        }
    }
    Trx_End(&hierarchy->trx_state, &data.trx_cur);

    return err;
}

/**
 * Compress a subtree using DFS starting from node.
 * @returns The compressed tree is returned as a compressed_rms structure.
 */
static struct compressed_rms *compress_subtree(RedisModuleCtx *ctx, SelvaHierarchy *hierarchy, struct SelvaHierarchyNode *node) {
    RedisModuleString *raw;
    struct compressed_rms *compressed;
    int err;

    err =verifyDetachableSubtree(hierarchy, node);
    if (err) {
        /* Not a valid subtree. */
        fprintf(stderr, "%s:%d: %.*s is not a valid subtree for compression: %s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, node->id,
                getSelvaErrorStr(err));
        return NULL;
    }

    compressed = rms_alloc_compressed();
    if (!compressed) {
        fprintf(stderr, "%s:%d: Failed to allocate memory for a compressed subtree of %.*s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, node->id);
        /* ENOMEM */
        return NULL;
    }

    struct SelvaHierarchySubtree subtree = {
        .hierarchy = hierarchy,
        .node = node,
    };
    raw = RedisModule_SaveDataTypeToString(ctx, &subtree, HierarchySubtreeType);
    if (!raw) {
        rms_free_compressed(compressed);

        return NULL;
    }

    err = rms_compress(compressed, raw);
    RedisModule_FreeString(ctx, raw);
    if (err) {
        rms_free_compressed(compressed);
        fprintf(stderr, "%s:%d: Failed to compress the subtree of %.*s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, node->id);

        return NULL;
    }

    return compressed;
}

static int detach_subtree(RedisModuleCtx *ctx, SelvaHierarchy *hierarchy, struct SelvaHierarchyNode *node) {
    Selva_NodeId node_id;
    struct compressed_rms *compressed;
    Selva_NodeId *parents = NULL;
    const size_t nr_parents = SVector_Size(&node->parents);
    int err;

    if (node->flags.detached) {
        fprintf(stderr, "%s:%d: Node already detached: %.*s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, node->id);
        return SELVA_HIERARCHY_EINVAL;
    }

    if (nr_parents > 0) {
        parents = RedisModule_PoolAlloc(ctx, nr_parents * SELVA_NODE_ID_SIZE);
        if (!parents) {
            fprintf(stderr, "%s:%d: Failed to allocate memory for detaching %.*s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node->id);
            return SELVA_HIERARCHY_ENOMEM;
        }

        copy_nodeIds(parents, &node->parents);
    }

    compressed = compress_subtree(ctx, hierarchy, node);
    if (!compressed) {
        /* TODO Maybe it would be nice to get the error codes passed here. */
        return SELVA_HIERARCHY_EGENERAL;
    }

    memcpy(node_id, node->id, SELVA_NODE_ID_SIZE);
    err = SelvaModify_DelHierarchyNodeP(ctx, hierarchy, node, DEL_HIERARCHY_NODE_FORCE | DEL_HIERARCHY_NODE_DETACH, compressed);
    node = NULL;
    /*
     * Note that `compressed` must not be freed as it's actually stored now in
     * the detached hierarchy for now.
     */

    /*
     * Create a new dummy node with the detached flag set.
     */
    new_detached_head(ctx, hierarchy, node_id, parents, nr_parents);

    fprintf(stderr, "%s:%d: Compressed and detached the subtree of %.*s: %s\n",
            __FILE__, __LINE__,
            (int)SELVA_NODE_ID_SIZE, node_id,
            getSelvaErrorStr(err));

    return err;
}

/**
 * Restore a compressed subtree back to hierarchy from the detached hierarchy subtree storage.
 * @param id can be the id of any node within a compressed subtree.
 * @returns SELVA_ENOENT if id is not a member of any detached subtree.
 */
static int restore_subtree(SelvaHierarchy *hierarchy, const Selva_NodeId id) {
    struct compressed_rms *compressed;
    RedisModuleString *uncompressed;
    const void *load_res;
    int err;

    SELVA_TRACE_BEGIN(restore_subtree);

    err = SelvaHierarchyDetached_Get(hierarchy, id, &compressed);
    if (err) {
        return err;
    }

    err = rms_decompress(&uncompressed, compressed);
    if (err) {
        return err;
    }

    isDecompressingSubtree = 1;
    subtree_hierarchy = hierarchy;
    load_res = RedisModule_LoadDataTypeFromString(uncompressed, HierarchySubtreeType);
    isDecompressingSubtree = 0;
    rms_free_compressed(compressed); /* All occurrences of the pointer should have been cleaned by now. */
    RedisModule_FreeString(NULL, uncompressed);
    if (!load_res) {
        return SELVA_HIERARCHY_EINVAL;
    }

    SELVA_TRACE_END(restore_subtree);

    fprintf(stderr, "%s:%d: Restored the subtree of %.*s\n",
            __FILE__, __LINE__,
            (int)SELVA_NODE_ID_SIZE, id);

    return 0;
}

void HierarchyTypeFree(void *value) {
    SelvaHierarchy *hierarchy = (SelvaHierarchy *)value;

    SelvaModify_DestroyHierarchy(hierarchy);
}

/*
 * SELVA.HIERARCHY.DEL HIERARCHY_KEY FLAGS [NODE_ID1[, NODE_ID2, ...]]
 * If no NODE_IDs are given then nothing will be deleted.
 */
int SelvaHierarchy_DelNodeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    const char *flags = RedisModule_StringPtrLen(argv[2], NULL);

    long long nr_deleted = 0;
    for (int i = 3; i < argc; i++) {
        Selva_NodeId nodeId;

        Selva_RMString2NodeId(nodeId, argv[i]);

        if (!SelvaModify_DelHierarchyNode(ctx, hierarchy, nodeId, flags[0] == 'F' ? DEL_HIERARCHY_NODE_FORCE : 0)) {
            nr_deleted++;
        }
    }

    RedisModule_ReplyWithLongLong(ctx, nr_deleted);
    RedisModule_ReplicateVerbatim(ctx);
    SelvaSubscriptions_SendDeferredEvents(hierarchy);

    return REDISMODULE_OK;
}

int SelvaHierarchy_HeadsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    const SelvaHierarchy *hierarchy;

    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    struct SVectorIterator it;
    const SelvaHierarchyNode *node;

    RedisModule_ReplyWithArray(ctx, SVector_Size(&hierarchy->heads));

    SVector_ForeachBegin(&it, &hierarchy->heads);
    while ((node = SVector_Foreach(&it))) {
        RedisModule_ReplyWithStringBuffer(ctx, node->id, Selva_NodeIdLen(node->id));
    }

    return REDISMODULE_OK;
}

int SelvaHierarchy_ParentsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    Selva_RMString2NodeId(nodeId, argv[2]);
    const SelvaHierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
    }

    struct SVectorIterator it;
    const SelvaHierarchyNode *parent;
    const SVector *parents;

#if HIERARCHY_SORT_BY_DEPTH
    SVECTOR_AUTOFREE(parents_d);

    if (unlikely(!SVector_Clone(&parents_d, &node->parents, SVector_HierarchyNode_depth_compare))) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOMEM);
    }

    parents = &parents_d;
#else
    parents = &node->parents;
#endif

    RedisModule_ReplyWithArray(ctx, SVector_Size(parents));

    SVector_ForeachBegin(&it, parents);
    while ((parent = SVector_Foreach(&it))) {
        RedisModule_ReplyWithStringBuffer(ctx, parent->id, Selva_NodeIdLen(parent->id));
    }

    return REDISMODULE_OK;
}

int SelvaHierarchy_ChildrenCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    Selva_RMString2NodeId(nodeId, argv[2]);
    const SelvaHierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
    }

    RedisModule_ReplyWithArray(ctx, SVector_Size(&node->children));

    struct SVectorIterator it;
    const SelvaHierarchyNode *child;

    SVector_ForeachBegin(&it, &node->children);
    while((child = SVector_Foreach(&it))) {
        RedisModule_ReplyWithStringBuffer(ctx, child->id, Selva_NodeIdLen(child->id));
    }

    return REDISMODULE_OK;
}

int SelvaHierarchy_EdgeListCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3 && argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    Selva_RMString2NodeId(nodeId, argv[2]);
    SelvaHierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
    }

    struct SelvaObject *obj = node->metadata.edge_fields.edges;

    if (!obj) {
        /* No custom edges set. */
        return RedisModule_ReplyWithArray(ctx, 0);
    }

    const RedisModuleString *key_name = argc == 4 ? argv[3] : NULL;
    if (key_name) {
        int err;

        err = SelvaObject_GetObject(obj, key_name, &obj);
        if (err) {
            return replyWithSelvaError(ctx, err);
        }
    }

    SelvaObject_ReplyWithObject(ctx, NULL, obj, NULL, 0);

    return REDISMODULE_OK;
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
int SelvaHierarchy_EdgeGetCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;
    const SelvaHierarchyNode *node;

    Selva_RMString2NodeId(nodeId, argv[2]);
    node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
    }

    const RedisModuleString *field_name = argv[3];
    TO_STR(field_name);
    const struct EdgeField *edge_field;

    edge_field = Edge_GetField(node, field_name_str, field_name_len);
    if (!edge_field) {
        return RedisModule_ReplyWithNull(ctx);
    }

    const struct SVector *arcs = &edge_field->arcs;
    struct SVectorIterator it;
    const SelvaHierarchyNode *dst;

    RedisModule_ReplyWithArray(ctx, 1 + SVector_Size(arcs));
    RedisModule_ReplyWithLongLong(ctx, edge_field->constraint ? edge_field->constraint->constraint_id : EDGE_FIELD_CONSTRAINT_ID_DEFAULT);

    SVector_ForeachBegin(&it, arcs);
    while ((dst = SVector_Foreach(&it))) {
        RedisModule_ReplyWithStringBuffer(ctx, dst->id, Selva_NodeIdLen(dst->id));
    }

    return REDISMODULE_OK;
}

/*
 * Get metadata of an edge.
 *
 * Reply format:
 * SelvaObject
 */
int SelvaHierarchy_EdgeGetMetadataCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    Selva_NodeId src_node_id;
    const RedisModuleString *field_name;
    Selva_NodeId dst_node_id;
    int err;

    if (argc != 5) {
        return RedisModule_WrongArity(ctx);
    }

    Selva_RMString2NodeId(src_node_id, argv[2]);
    field_name = argv[3];
    Selva_RMString2NodeId(dst_node_id, argv[4]);
    TO_STR(field_name);

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Find the node.
     */
    const SelvaHierarchyNode *src_node = SelvaHierarchy_FindNode(hierarchy, src_node_id);
    if (!src_node) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
    }

    struct EdgeField *edge_field;
    edge_field = Edge_GetField(src_node, field_name_str, field_name_len);
    if (!edge_field) {
        return replyWithSelvaErrorf(ctx, SELVA_ENOENT, "Edge field not found");
    }

    struct SelvaObject *edge_metadata;
    err = Edge_GetFieldEdgeMetadata(edge_field, dst_node_id, 0, &edge_metadata);
    if (err == SELVA_ENOENT) {
        return RedisModule_ReplyWithNull(ctx);
    } else if (err) { /* Also if the edge doesn't exist. */
        return replyWithSelvaError(ctx, err);
    }

    SelvaObject_ReplyWithObject(ctx, NULL, edge_metadata, NULL, SELVA_OBJECT_REPLY_BINUMF_FLAG);

    return REDISMODULE_OK;
}

int SelvaHierarchy_CompressCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    Selva_RMString2NodeId(nodeId, argv[2]);
    SelvaHierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
    }

    err = detach_subtree(ctx, hierarchy, node);
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    RedisModule_ReplyWithLongLong(ctx, 1);
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
}

int SelvaHierarchy_ListCompressedCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    SelvaObject_Iterator *it;
    const char *id;

    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    if (!hierarchy->index_detached.obj) {
        return RedisModule_ReplyWithNull(ctx);
    }

    RedisModule_ReplyWithArray(ctx, SelvaObject_Len(hierarchy->index_detached.obj, NULL));
    it = SelvaObject_ForeachBegin(hierarchy->index_detached.obj);
    while ((id = SelvaObject_ForeachKey(hierarchy->index_detached.obj, &it))) {
        RedisModule_ReplyWithSimpleString(ctx, id);
    }

    return REDISMODULE_OK;
}

static int SelvaVersion_AuxLoad(RedisModuleIO *io, int encver __unused, int when __unused) {
    selva_db_version_info.created_with = RedisModule_LoadString(io);
    selva_db_version_info.updated_with = RedisModule_LoadString(io);

    fprintf(stderr, "Selva hierarchy version info created_with: %s updated_with: %s\n",
            RedisModule_StringPtrLen(selva_db_version_info.created_with, NULL),
            RedisModule_StringPtrLen(selva_db_version_info.updated_with, NULL));

    return 0;
}

static void SelvaVersion_AuxSave(RedisModuleIO *io, int when __unused) {
    if (selva_db_version_info.created_with) {
        RedisModule_SaveString(io, selva_db_version_info.created_with);
    } else {
        RedisModule_SaveStringBuffer(io, selva_version, strlen(selva_version));
    }
    RedisModule_SaveStringBuffer(io, selva_version, strlen(selva_version));
}

static int Hierarchy_OnLoad(RedisModuleCtx *ctx) {
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
        return REDISMODULE_ERR;
    }

    HierarchySubtreeType = RedisModule_CreateDataType(ctx, "hisubtree", HIERARCHY_ENCODING_VERSION, &ztm);
    if (HierarchyType == NULL) {
        return REDISMODULE_ERR;
    }

    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.del", SelvaHierarchy_DelNodeCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.heads", SelvaHierarchy_HeadsCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.parents", SelvaHierarchy_ParentsCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.children", SelvaHierarchy_ChildrenCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.edgelist", SelvaHierarchy_EdgeListCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.edgeget", SelvaHierarchy_EdgeGetCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.edgegetmetadata", SelvaHierarchy_EdgeGetMetadataCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.compress", SelvaHierarchy_CompressCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.listcompressed", SelvaHierarchy_ListCompressedCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Hierarchy_OnLoad);
