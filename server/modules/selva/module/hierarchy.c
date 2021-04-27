#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "redismodule.h"
#include "alias.h"
#include "async_task.h"
#include "cdefs.h"
#include "errors.h"
#include "hierarchy.h"
#include "edge.h"
#include "modify.h"
#include "rpn.h"
#include "selva_node.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "subscriptions.h"
#include "svector.h"

typedef struct SelvaModify_HierarchyNode {
    Selva_NodeId id;
    struct trx trx_label;
#if HIERARCHY_SORT_BY_DEPTH
    ssize_t depth;
#endif
    struct SelvaModify_HierarchyMetadata metadata;
    SVector parents;
    SVector children;
    RB_ENTRY(SelvaModify_HierarchyNode) _index_entry;
} SelvaModify_HierarchyNode;

typedef struct SelvaModify_HierarchySearchFilter {
    Selva_NodeId id;
} SelvaModify_HierarchySearchFilter;

enum SelvaModify_HierarchyNode_Relationship {
    RELATIONSHIP_PARENT,
    RELATIONSHIP_CHILD,
};

typedef void (*HierarchyNode_HeadCallback)(SelvaModify_HierarchyNode *node, void *arg);

/**
 * Called for each node found during a traversal.
 * @returns 0 to continue the traversal; 1 to interrupt the traversal.
 */
typedef int (*HierarchyNode_Callback)(SelvaModify_HierarchyNode *node, void *arg);

typedef void (*HierarchyNode_ChildCallback)(SelvaModify_HierarchyNode *parent, SelvaModify_HierarchyNode *child, void *arg);

typedef struct TraversalCallback {
    /**
     * Called for each orphan head in the hierarchy.
     */
    HierarchyNode_HeadCallback head_cb;
    void * head_arg;

    /**
     * Called for each node in the hierarchy.
     */
    HierarchyNode_Callback node_cb;
    void * node_arg;

    /**
     * Called for each child of current node.
     */
    HierarchyNode_ChildCallback child_cb;
    void * child_arg;
} TraversalCallback;

/* Node metadata constructors. */
SET_DECLARE(selva_HMCtor, SelvaModify_HierarchyMetadataHook);
/* Node metadata destructors. */
SET_DECLARE(selva_HMDtor, SelvaModify_HierarchyMetadataHook);

__nonstring static const Selva_NodeId HIERARCHY_RDB_EOF;
static RedisModuleType *HierarchyType;

#if HIERARCHY_SORT_BY_DEPTH
/*!<
 * DB is loading.
 * If set then some expensive operations can be skipped and/or deferred.
 */
static int rdbLoading;
#endif

static void SelvaModify_DestroyNode(SelvaModify_HierarchyNode *node);
static void removeRelationships(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node, enum SelvaModify_HierarchyNode_Relationship rel);
RB_PROTOTYPE_STATIC(hierarchy_index_tree, SelvaModify_HierarchyNode, _index_entry, SelvaModify_HierarchyNode_Compare)

static int SVector_HierarchyNode_id_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const SelvaModify_HierarchyNode *a = *(const SelvaModify_HierarchyNode **)a_raw;
    const SelvaModify_HierarchyNode *b = *(const SelvaModify_HierarchyNode **)b_raw;

    assert(a);
    assert(b);

    return memcmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

#if HIERARCHY_SORT_BY_DEPTH
static int SVector_HierarchyNode_depth_compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const SelvaModify_HierarchyNode *a = *(const SelvaModify_HierarchyNode **)a_raw;
    const SelvaModify_HierarchyNode *b = *(const SelvaModify_HierarchyNode **)b_raw;

    assert(a);
    assert(b);

    return b->depth - a->depth;
}
#endif

static int SelvaModify_HierarchyNode_Compare(const SelvaModify_HierarchyNode *a, const SelvaModify_HierarchyNode *b) {
    return memcmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

RB_GENERATE_STATIC(hierarchy_index_tree, SelvaModify_HierarchyNode, _index_entry, SelvaModify_HierarchyNode_Compare)

/**
 * Wrap RedisModule_Free().
 */
static void wrapFree(void *p) {
    void **pp = (void **)p;

    RedisModule_Free(*pp);
}

static inline void RMString2NodeId(Selva_NodeId nodeId, RedisModuleString *rmStr) {
    Selva_NodeIdCpy(nodeId, RedisModule_StringPtrLen(rmStr, NULL));
}

SelvaModify_Hierarchy *SelvaModify_NewHierarchy(RedisModuleCtx *ctx) {
    SelvaModify_Hierarchy *hierarchy;

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

    /*
     * Subscriptions.
     * TODO It might make sense to move these to subscriptions.c
     */
    RB_INIT(&hierarchy->subs.head);
    hierarchy->subs.missing = SelvaObject_New();
    if (!hierarchy->subs.missing) {
        SelvaModify_DestroyHierarchy(hierarchy);
        hierarchy = NULL;
        goto fail;
    }
    if (SelvaSubscriptions_InitMarkersStruct(&hierarchy->subs.detached_markers)) {
        SelvaModify_DestroyHierarchy(hierarchy);
        hierarchy = NULL;
        goto fail;
    }
    if (SelvaSubscriptions_InitDeferredEvents(hierarchy)) {
        SelvaModify_DestroyHierarchy(hierarchy);
        hierarchy = NULL;
        goto fail;
    }

    if(unlikely(SelvaModify_SetHierarchy(ctx, hierarchy, ROOT_NODE_ID, 0, NULL, 0, NULL) < 0)) {
        SelvaModify_DestroyHierarchy(hierarchy);
        hierarchy = NULL;
        goto fail;
    }

fail:
    return hierarchy;
}

void SelvaModify_DestroyHierarchy(SelvaModify_Hierarchy *hierarchy) {
    SelvaModify_HierarchyNode *node;
    SelvaModify_HierarchyNode *next;

	for (node = RB_MIN(hierarchy_index_tree, &hierarchy->index_head); node != NULL; node = next) {
		next = RB_NEXT(hierarchy_index_tree, &hierarchy->index_head, node);
		RB_REMOVE(hierarchy_index_tree, &hierarchy->index_head, node);
        SelvaModify_DestroyNode(node);
    }

    SelvaSubscriptions_DestroyAll(hierarchy);

    SVector_Destroy(&hierarchy->heads);
#if MEM_DEBUG
    memset(hierarchy, 0, sizeof(*hierarchy));
#endif
    RedisModule_Free(hierarchy);
}

SelvaModify_Hierarchy *SelvaModify_OpenHierarchy(RedisModuleCtx *ctx, RedisModuleString *key_name, int mode) {
    SelvaModify_Hierarchy *hierarchy = NULL;
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

static int create_node_object(RedisModuleCtx *ctx, const Selva_NodeId id) {
    RedisModuleString *node_name;
    RedisModuleKey *key;

    node_name = RedisModule_CreateStringPrintf(ctx, "%.*s", (int)SELVA_NODE_ID_SIZE, id);
    if (unlikely(!node_name)) {
        return SELVA_HIERARCHY_ENOMEM;
    }

    /*
     * Open the redis key.
     */
    key = RedisModule_OpenKey(ctx, node_name, REDISMODULE_READ | REDISMODULE_WRITE);
    if (!key) {
        fprintf(stderr, "%s:%d: Failed to open key: %.*s\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE,
                id);
        return SELVA_HIERARCHY_ENOMEM; // TODO ??
    }

    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        int err;

        err = SelvaNode_Initialize(ctx, key, node_name, id);
        if (err) {
            fprintf(stderr, "%s:%d: Node initialization error: %s\n",
                    __FILE__, __LINE__,
                    getSelvaErrorStr(err));
            RedisModule_CloseKey(key);
            return err;
        }
    }

    /* We don't need to access the node for now, so just close it immediately. */
    RedisModule_CloseKey(key);

    return 0;
}

static SelvaModify_HierarchyNode *newNode(RedisModuleCtx *ctx, const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node = RedisModule_Calloc(1, sizeof(SelvaModify_HierarchyNode));
    if (unlikely(!node)) {
        return NULL;
    };

#if 0
    fprintf(stderr, "%s:%d: Creating node %.*s\n",
            __FILE__, __LINE__,
            (int)SELVA_NODE_ID_SIZE, id);
#endif

    if (unlikely(!SVector_Init(&node->parents, HIERARCHY_INITIAL_VECTOR_LEN, SVector_HierarchyNode_id_compare) ||
                 !SVector_Init(&node->children, HIERARCHY_INITIAL_VECTOR_LEN, SVector_HierarchyNode_id_compare))) {
        SelvaModify_DestroyNode(node);
        return NULL;
    }

    memcpy(node->id, id, SELVA_NODE_ID_SIZE);

    if (likely(ctx)) {
        int err;

        err = create_node_object(ctx, id);
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

    SelvaModify_HierarchyMetadataHook **metadata_ctor_p;

    SET_FOREACH(metadata_ctor_p, selva_HMCtor) {
        SelvaModify_HierarchyMetadataHook *ctor = *metadata_ctor_p;
        ctor(node->id, &node->metadata);
    }

    return node;
}

static void SelvaModify_DestroyNode(SelvaModify_HierarchyNode *node) {
    SelvaModify_HierarchyMetadataHook **dtor_p;

    SET_FOREACH(dtor_p, selva_HMDtor) {
        SelvaModify_HierarchyMetadataHook *dtor = *dtor_p;
        dtor(node->id, &node->metadata);
    }

    SVector_Destroy(&node->parents);
    SVector_Destroy(&node->children);
#if MEM_DEBUG
    memset(node, 0, sizeof(*node));
#endif
    RedisModule_Free(node);
}

SelvaModify_HierarchyNode *SelvaHierarchy_FindNode(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
    SelvaModify_HierarchySearchFilter filter;

    memcpy(&filter.id, id, SELVA_NODE_ID_SIZE);
    return RB_FIND(hierarchy_index_tree, &hierarchy->index_head, (SelvaModify_HierarchyNode *)(&filter));
}

int SelvaModify_HierarchyNodeExists(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
    return SelvaHierarchy_FindNode(hierarchy, id) != NULL;
}

void SelvaModify_HierarchyGetNodeId(Selva_NodeId id, const SelvaModify_HierarchyNode *node) {
    memcpy(id, node->id, SELVA_NODE_ID_SIZE);
}

/* TODO Rename these functions? */
struct SelvaModify_HierarchyMetadata *SelvaModify_HierarchyGetNodeMetadataByPtr(SelvaModify_HierarchyNode *node) {
    return &node->metadata;
}

struct SelvaModify_HierarchyMetadata *SelvaModify_HierarchyGetNodeMetadata(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return NULL;
    }

    return &node->metadata;
}

static inline void mkHead(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node) {
    (void)SVector_InsertFast(&hierarchy->heads, node);
}

static inline void rmHead(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node) {
    SVector_Remove(&hierarchy->heads, node);
}

static void del_node(RedisModuleCtx *ctx, SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node) {
    Selva_NodeId id;
    int is_root;

    memcpy(id, node->id, SELVA_NODE_ID_SIZE);
    is_root = !memcmp(id, ROOT_NODE_ID, SELVA_NODE_ID_SIZE);

    if (likely(ctx)) {
        Selva_Subscriptions_DeferTriggerEvents(ctx, hierarchy, id, SELVA_SUBSCRIPTION_TRIGGER_TYPE_DELETED);
    }

    removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);
    SelvaSubscriptions_DeferHierarchyDeletionEvents(hierarchy, id, &node->metadata);

    /*
     * Never delete the root node.
     */
    if (!is_root) {
        removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);

        /*
         * The node was now marked as a head but we are going to get rid of it
         * soon, so there is no reason to make it a tree head. In fact, doing
         * so would break things.
         */
        rmHead(hierarchy, node);

        RB_REMOVE(hierarchy_index_tree, &hierarchy->index_head, node);
        SelvaModify_DestroyNode(node);
    }

    if (likely(ctx)) {
        RedisModuleString *rms_id;

        rms_id = RedisModule_CreateString(ctx, id, Selva_NodeIdLen(id));
        SelvaNode_Delete(ctx, rms_id);

        if (is_root) {
            create_node_object(ctx, id);
        }
    }
}

#if HIERARCHY_SORT_BY_DEPTH
static void updateDepth(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *head) {
    SVector q;

    if (unlikely(rdbLoading)) {
        /*
         * Skip updates for now as it would require a full BFS pass for every new node.
         */
        return;
    }

    if (unlikely(!SVector_Init(&q, HIERARCHY_EXPECTED_RESP_LEN, NULL))) {
        fprintf(stderr, "%s:%d: Depth update error\n", __FILE__, __LINE__);
        abort();
    }

    Trx_Begin(&hierarchy->current_trx);
    Trx_Stamp(&hierarchy->current_trx, &head->visit_stamp);
    (void)SVector_InsertFast(&q, head);

    while (SVector_Size(&q) > 0) {
        SelvaModify_HierarchyNode *node = SVector_Shift(&q);
        struct SVectorIterator it;

        /*
         * Update the depth.
         */
        ssize_t new_depth = 0;
        SelvaModify_HierarchyNode *parent;
        SVector_ForeachBegin(&it, &node->parents);
        while ((parent = SVector_Foreach(&it))) {
            new_depth = max(new_depth, parent->depth + 1);
        }
        node->depth = new_depth;

        SelvaModify_HierarchyNode *child;
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
ssize_t SelvaModify_GetHierarchyDepth(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
    const SelvaModify_HierarchyNode *node;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return -1;
    }

    return node->depth;
}
#endif

static inline void publishAncestorsUpdate(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        const SelvaModify_HierarchyNode *node) {
    if (ctx) {
        SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, node->id, &node->metadata, "ancestors");
    }
}

static inline void publishDescendantsUpdate(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        const SelvaModify_HierarchyNode *node) {
    if (ctx) {
        SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, node->id, &node->metadata, "descendants");
    }
}

static inline void publishChildrenUpdate(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        const SelvaModify_HierarchyNode *node) {
    if (ctx) {
        SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, node->id, &node->metadata, "children");
    }
}

static inline void publishParentsUpdate(
        RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        const SelvaModify_HierarchyNode *node) {
    if (ctx) {
        SelvaSubscriptions_DeferFieldChangeEvents(ctx, hierarchy, node->id, &node->metadata, "parents");
    }
}

static int cross_insert_children(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *node,
        size_t n,
        const Selva_NodeId *nodes) {
    int res = 0;

    if (n == 0) {
        return 0; /* No changes. */
    }

    SelvaSubscriptions_DeferHierarchyEvents(hierarchy, node->id, &node->metadata);

    for (size_t i = 0; i < n; i++) {
        SelvaModify_HierarchyNode *adjacent;

        adjacent = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
        if (!adjacent) {
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

            adjacent = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
            if (!adjacent) {
                fprintf(stderr, "%s:%d: Node state error, node: \"%.*s\"\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, nodes[i]);
                return SELVA_HIERARCHY_EGENERAL;
            }
        }

        /* The adjacent node is no longer an orphan */
        if (SVector_Size(&adjacent->parents) == 0) {
            rmHead(hierarchy, adjacent);
        }

        if (SVector_InsertFast(&node->children, adjacent) == NULL) {
            (void)SVector_InsertFast(&adjacent->parents, node);

#if 0
            fprintf(stderr, "%s:%d: Inserted %.*s.children <= %.*s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node->id,
                    (int)SELVA_NODE_ID_SIZE, adjacent->id);
#endif

            /*
             * Publish that the parents field was changed.
             * Actual events are only sent if there are subscription markers
             * set on this node.
             */
            publishParentsUpdate(ctx, hierarchy, adjacent);
            publishAncestorsUpdate(ctx, hierarchy, adjacent);
            SelvaSubscriptions_InheritChild(
                hierarchy,
                node->id, &node->metadata,
                SVector_Size(&node->parents),
                adjacent->id, &adjacent->metadata);
            res++; /* Count actual insertions */
        }

        publishChildrenUpdate(ctx, hierarchy, node);
        publishDescendantsUpdate(ctx, hierarchy, node);
    }

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
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *node,
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

    SelvaSubscriptions_DeferHierarchyEvents(hierarchy, node->id, &node->metadata);

    for (size_t i = 0; i < n; i++) {
        SelvaModify_HierarchyNode *adjacent;

        adjacent = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
        if (!adjacent) {
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

            adjacent = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
            if (!adjacent) {
                fprintf(stderr, "%s:%d: Node state error, node: \"%.*s\"\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, nodes[i]);
                return SELVA_HIERARCHY_EGENERAL;
            }
        }

        /* Do inserts only if the relationship doesn't exist already */
        if (SVector_InsertFast(&node->parents, adjacent) == NULL) {
            (void)SVector_InsertFast(&adjacent->children, node);

#if 0
            fprintf(stderr, "%s:%d: Inserted %.*s.parents <= %.*s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node->id,
                    (int)SELVA_NODE_ID_SIZE, adjacent->id);
#endif

            /*
             * Publish that the children field was changed.
             * Actual events are only sent if there are subscription markers
             * set on this node.
             */
            publishChildrenUpdate(ctx, hierarchy, adjacent);
            publishDescendantsUpdate(ctx, hierarchy, adjacent);
            SelvaSubscriptions_InheritParent(
                hierarchy,
                node->id, &node->metadata,
                SVector_Size(&node->children),
                adjacent->id, &adjacent->metadata);
            res++;
        }
    }

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
 * TODO This should be probably refactored into two functions or something to make it cleaner.
 */
static int crossRemove(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *node,
        enum SelvaModify_HierarchyNode_Relationship rel,
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
#endif

    SelvaSubscriptions_DeferHierarchyEvents(hierarchy, node->id, &node->metadata);
    SelvaSubscriptions_ClearAllMarkers(hierarchy, node->id, &node->metadata);

    if (rel == RELATIONSHIP_CHILD) { /* no longer a child of adjacent */
        const size_t initialNodeParentsSize = SVector_Size(&node->parents);
        int pubParents = 0;

        for (size_t i = 0; i < n; i++) {
            SelvaModify_HierarchyNode *adjacent;

            if (pointers) {
                memcpy(&adjacent, nodes[i], sizeof(SelvaModify_HierarchyNode *));
            } else {
                adjacent = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
            }

            if (!adjacent) {
                /*
                 * The most Redis thing to do is probably to ignore any
                 * missing nodes.
                 */
                continue;
            }

            SVector_Remove(&adjacent->children, node);
            SVector_Remove(&node->parents, adjacent);

#if HIERARCHY_SORT_BY_DEPTH
            updateDepth(hierarchy, adjacent);
#endif
            publishChildrenUpdate(ctx, hierarchy, adjacent);
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
            SelvaModify_HierarchyNode *adjacent;

            if (pointers) {
                memcpy(&adjacent, nodes[i], sizeof(SelvaModify_HierarchyNode *));
            } else {
                adjacent = SelvaHierarchy_FindNode(hierarchy, nodes[i]);
            }

            if (!adjacent) {
                /*
                 * The most Redis thing to do is probably to ignore any
                 * missing nodes.
                 */
                continue;
            }

            SVector_Remove(&adjacent->parents, node);
            SVector_Remove(&node->children, adjacent);

            if (SVector_Size(&adjacent->parents) == 0) {
                /* adjacent is an orphan now */
                mkHead(hierarchy, adjacent);
            }

#if HIERARCHY_SORT_BY_DEPTH
            updateDepth(hierarchy, adjacent);
#endif
            publishParentsUpdate(ctx, hierarchy, adjacent);
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
    SelvaSubscriptions_RefreshByMarker(hierarchy, &sub_markers);

    return 0;
}

/**
 * Remove all relationships rel of node.
 */
static void removeRelationships(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node, enum SelvaModify_HierarchyNode_Relationship rel) {
    size_t offset_a;
    size_t offset_b;
    SVECTOR_AUTOFREE(sub_markers);

    switch (rel) {
    case RELATIONSHIP_PARENT:
        /* Remove parent releationship to other nodes */
        offset_a = offsetof(SelvaModify_HierarchyNode, children);
        offset_b = offsetof(SelvaModify_HierarchyNode, parents);
        break;
    case RELATIONSHIP_CHILD:
        /* Remove child releationship to other nodes */
        offset_a = offsetof(SelvaModify_HierarchyNode, parents);
        offset_b = offsetof(SelvaModify_HierarchyNode, children);
        break;
    default:
        assert(("rel is invalid", 0));
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

    SelvaSubscriptions_DeferHierarchyEvents(hierarchy, node->id, &node->metadata);
    SelvaSubscriptions_ClearAllMarkers(hierarchy, node->id, &node->metadata);

    struct SVectorIterator it;
    SelvaModify_HierarchyNode *adj;
    SVector *vec_a = (SVector *)((char *)node + offset_a);

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

    /*
     * After this the caller should call mkHead(hierarchy, node)
     * if rel == RELATIONSHIP_CHILD.
     */
}

int SelvaModify_DelHierarchyChildren(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node;

    node  = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);

    return 0;
}

int SelvaModify_DelHierarchyParents(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);

    return 0;
}

int SelvaModify_SetHierarchy(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaModify_HierarchyNode *node;
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
            SelvaModify_DestroyNode(node);

            return SELVA_HIERARCHY_EEXIST;
        }
        res++;
    } else {
        /* Clear the existing node relationships */
        removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);
        removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);
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

static int remove_missing(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *node,
        size_t nr_nodes,
        const Selva_NodeId *nodes,
        enum SelvaModify_HierarchyNode_Relationship rel) {
    SVECTOR_AUTOFREE(old_adjs);
    struct SVectorIterator it;
    SelvaModify_HierarchyNode *adj;
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

            memcpy(arr, &adj, sizeof(SelvaModify_HierarchyNode *));
            crossRemove(ctx, hierarchy, node, rel, 1, arr, 1);
            res++;
        }
    }

    return res;
}

int SelvaModify_SetHierarchyParents(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents) {
    SelvaModify_HierarchyNode *node;
    int err, res = 0;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    if (nr_parents == 0) {
        /* Clear the existing node relationships. */
        removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);

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
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaModify_HierarchyNode *node;
    int err, res = 0;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    if (nr_children == 0) {
        /* Clear the existing node relationships */
        removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);

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

static int isRdbLoading(RedisModuleCtx *ctx) {
     return !!(REDISMODULE_CTX_FLAGS_LOADING & RedisModule_GetContextFlags(ctx));
}

int SelvaHierarchy_UpsertNode(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        SelvaModify_HierarchyNode **out) {
    SelvaModify_HierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, id);
    int isLoading = isRdbLoading(ctx);

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
     if (RB_INSERT(hierarchy_index_tree, &hierarchy->index_head, node) != NULL) {
         /*
          * We are being extremely paranoid here as this shouldn't be possible.
          */
         SelvaModify_DestroyNode(node);

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

int SelvaModify_AddHierarchy(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaModify_HierarchyNode *node;
    int isNewNode;
    int err, res = 0;

    err = SelvaHierarchy_UpsertNode(ctx, hierarchy, id, &node);
    if (err == SELVA_HIERARCHY_EEXIST) {
        isNewNode = 0;
    } else if (err) {
        return err;
    } else {
        isNewNode = 1;
        res++;
    }

    /*
     * Update relationship relative to other nodes
     * RFE if isNewNode == 0 then errors are not handled properly as
     * we don't know how to rollback.
     */
    err = cross_insert_parents(ctx, hierarchy, node, nr_parents, parents);
    if (err < 0 && isNewNode) {
        del_node(ctx, hierarchy, node);
        return err;
    }
    res += err;

    /* Same for the children */
    err = cross_insert_children(ctx, hierarchy, node, nr_children, children);
    if (err < 0 && isNewNode) {
        del_node(ctx, hierarchy, node);
        return err;
    }
    res += err;

    return res;
}

int SelvaModify_DelHierarchy(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaModify_HierarchyNode *node;
    int err1, err2;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    err1 = crossRemove(ctx, hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents, 0);
    err2 = crossRemove(ctx, hierarchy, node, RELATIONSHIP_PARENT, nr_children, children, 0);

    return err1 ? err1 : err2;
}

static Selva_NodeId *NodeList_New(int nr_nodes) {
    return RedisModule_Alloc(nr_nodes * sizeof(Selva_NodeId));
}

static Selva_NodeId *NodeList_Insert(Selva_NodeId *list, const Selva_NodeId id, size_t nr_nodes) {
    Selva_NodeId *newList = RedisModule_Realloc(list, nr_nodes * sizeof(Selva_NodeId));
    if (unlikely(!newList)) {
        RedisModule_Free(list);
        return NULL;
    }

    memcpy(newList + nr_nodes - 1, id, sizeof(Selva_NodeId));

    return newList;
}

static Selva_NodeId *getNodeIds(SVector *adjacent_nodes, size_t * nr_ids) {
    size_t i = 0;
    Selva_NodeId *ids;
    struct SVectorIterator it;
    SelvaModify_HierarchyNode *adj;

    ids = NodeList_New(SVector_Size(adjacent_nodes));
    if (!ids) {
        return NULL;
    }

    SVector_ForeachBegin(&it, adjacent_nodes);
    while ((adj = SVector_Foreach(&it))) {
        ids = NodeList_Insert(ids, adj->id, ++i);
    }

    *nr_ids = i;
    return ids;
}

static int SelvaModify_DelHierarchyNodeP(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *node) {
    Selva_NodeId *ids;
    size_t nr_ids;

    assert(("hierarchy must be set", hierarchy));
    assert(("node must be set", node));

    ids = getNodeIds(&node->children, &nr_ids);
    if (unlikely(!ids)) {
        return SELVA_HIERARCHY_ENOMEM;
    }

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
        SelvaModify_HierarchyNode *child = SelvaHierarchy_FindNode(hierarchy, nodeId);
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
        memcpy(arr, &child, sizeof(SelvaModify_HierarchyNode *));
        err = crossRemove(ctx, hierarchy, node, RELATIONSHIP_PARENT, 1, arr, 1);
        if (err) {
            return err;
        }

        /*
         * Recursively delete the child and its children if its parents field is
         * empty and no edge fields are pointing to it.
         */
        if (SVector_Size(&child->parents) == 0 && Edge_Refcount(child) == 0) {
            err = SelvaModify_DelHierarchyNodeP(ctx, hierarchy, child);
            if (err) {
                return err;
            }
        }
    }

    RedisModule_Free(ids);
    del_node(ctx, hierarchy, node);

    return 0;
}

int SelvaModify_DelHierarchyNode(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node;

    node = SelvaHierarchy_FindNode(hierarchy, id);
    if (!node) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return SelvaModify_DelHierarchyNodeP(ctx, hierarchy, node);
}

/*
 * TODO Unused function.
 */
ssize_t SelvaModify_GetHierarchyHeads(SelvaModify_Hierarchy *hierarchy, Selva_NodeId **res) {
    struct SVectorIterator it;
    SelvaModify_HierarchyNode *node;
    Selva_NodeId *list = NodeList_New(1);
    ssize_t nr_nodes = 0;

    SVector_ForeachBegin(&it, &hierarchy->heads);
    while ((node = SVector_Foreach(&it))) {
        list = NodeList_Insert(list, node->id, ++nr_nodes);
        if (unlikely(!list)) {
            return SELVA_HIERARCHY_ENOMEM;
        }
    }

    *res = list;
    return nr_nodes;
}

static void HierarchyNode_HeadCallback_Dummy(SelvaModify_HierarchyNode *node, void *arg) {
    REDISMODULE_NOT_USED(node);
    REDISMODULE_NOT_USED(arg);
}

static int HierarchyNode_Callback_Dummy(SelvaModify_HierarchyNode *node, void *arg) {
    REDISMODULE_NOT_USED(node);
    REDISMODULE_NOT_USED(arg);

    return 0;
}

static void HierarchyNode_ChildCallback_Dummy(SelvaModify_HierarchyNode *parent, SelvaModify_HierarchyNode *child, void *arg) {
    REDISMODULE_NOT_USED(parent);
    REDISMODULE_NOT_USED(child);
    REDISMODULE_NOT_USED(arg);
}

/**
 * DFS from a given head node towards its descendants or ancestors.
 */
static int dfs(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *head, enum SelvaModify_HierarchyNode_Relationship dir, const TraversalCallback * restrict cb) {
    size_t offset;
    HierarchyNode_HeadCallback head_cb = cb->head_cb ? cb->head_cb : HierarchyNode_HeadCallback_Dummy;
    HierarchyNode_Callback node_cb = cb->node_cb ? cb->node_cb : HierarchyNode_Callback_Dummy;
    HierarchyNode_ChildCallback child_cb = cb->child_cb ? cb->child_cb : HierarchyNode_ChildCallback_Dummy;

    switch (dir) {
    case RELATIONSHIP_PARENT:
        offset = offsetof(SelvaModify_HierarchyNode, parents);
        break;
    case RELATIONSHIP_CHILD:
        offset = offsetof(SelvaModify_HierarchyNode, children);
        break;
    default:
        return SELVA_HIERARCHY_ENOTSUP;
    }

    SVECTOR_AUTOFREE(stack);
    if (unlikely(!SVector_Init(&stack, HIERARCHY_EXPECTED_RESP_LEN, NULL))) {
        return SELVA_HIERARCHY_ENOMEM;
    }

    struct trx trx_cur;
    if (Trx_Begin(&hierarchy->trx_state, &trx_cur)) {
        return SELVA_HIERARCHY_ETRMAX;
    }

    SVector_Insert(&stack, head);
    head_cb(head, cb->head_arg);

    while (SVector_Size(&stack) > 0) {
        SelvaModify_HierarchyNode *node;

        node = SVector_Pop(&stack);
        if (Trx_Visit(&trx_cur, &node->trx_label)) {
            if (node_cb(node, cb->node_arg)) {
                Trx_End(&hierarchy->trx_state, &trx_cur);
                return 0;
            }

            /* Add parents/children of this node to the stack of unvisited nodes */
            struct SVectorIterator it;
            SelvaModify_HierarchyNode *adj;
            const SVector *vec = (SVector *)((char *)node + offset);

            SVector_ForeachBegin(&it, vec);
            while ((adj = SVector_Foreach(&it))) {

                child_cb(node, adj, cb->child_arg);

                /* Add to the stack of unvisited nodes */
                SVector_Insert(&stack, adj);
            }
        }
    }

    Trx_End(&hierarchy->trx_state, &trx_cur);
    return 0;
}

/**
 * Traverse through all nodes of the hierarchy from heads to leaves.
 */
static int full_dfs(SelvaModify_Hierarchy *hierarchy, const TraversalCallback * restrict cb) {
    SelvaModify_HierarchyNode **head;
    SVECTOR_AUTOFREE(stack);

    HierarchyNode_HeadCallback head_cb = cb->head_cb ? cb->head_cb : HierarchyNode_HeadCallback_Dummy;
    HierarchyNode_Callback node_cb = cb->node_cb ? cb->node_cb : HierarchyNode_Callback_Dummy;
    HierarchyNode_ChildCallback child_cb = cb->child_cb ? cb->child_cb : HierarchyNode_ChildCallback_Dummy;

    if (unlikely(!SVector_Init(&stack, HIERARCHY_EXPECTED_RESP_LEN, NULL))) {
        return SELVA_HIERARCHY_ENOMEM;
    }

    struct trx trx_cur;
    if (Trx_Begin(&hierarchy->trx_state, &trx_cur)) {
        return SELVA_HIERARCHY_ETRMAX;
    }

    struct SVectorIterator it;

    SVector_ForeachBegin(&it, &hierarchy->heads);
    while ((head = SVector_Foreach(&it))) {
        SVector_Insert(&stack, head);
        head_cb(*head, cb->head_arg);

        while (SVector_Size(&stack) > 0) {
            SelvaModify_HierarchyNode *node = SVector_Pop(&stack);

            if (Trx_Visit(&trx_cur, &node->trx_label)) {
                if (node_cb(node, cb->node_arg)) {
                    Trx_End(&hierarchy->trx_state, &trx_cur);
                    return 0;
                }

                struct SVectorIterator it2;
                SelvaModify_HierarchyNode *adj;

                SVector_ForeachBegin(&it2, &node->children);
                while ((adj = SVector_Foreach(&it2))) {
                    child_cb(node, adj, cb->child_arg);

                    /* Add to the stack of unvisited nodes */
                    SVector_Insert(&stack, adj);
                }
            }
        }
    }

    Trx_End(&hierarchy->trx_state, &trx_cur);
    return 0;
}

#define BFS_TRAVERSE(hierarchy, head, cb) \
    HierarchyNode_HeadCallback head_cb = (cb)->head_cb ? (cb)->head_cb : HierarchyNode_HeadCallback_Dummy; \
    HierarchyNode_Callback node_cb = (cb)->node_cb ? (cb)->node_cb : HierarchyNode_Callback_Dummy; \
    HierarchyNode_ChildCallback child_cb = (cb)->child_cb ? (cb)->child_cb : HierarchyNode_ChildCallback_Dummy; \
    \
    SVECTOR_AUTOFREE(q); \
    if (unlikely(!SVector_Init(&q, HIERARCHY_EXPECTED_RESP_LEN, NULL))) { \
        return SELVA_HIERARCHY_ENOMEM; \
    } \
    \
    struct trx trx_cur; \
    if (Trx_Begin(&(hierarchy)->trx_state, &trx_cur)) { \
        return SELVA_HIERARCHY_ETRMAX; \
    } \
    \
    Trx_Visit(&trx_cur, &(head)->trx_label); \
    SVector_Insert(&q, (head)); \
    head_cb((head), (cb)->head_arg); \
    while (SVector_Size(&q) > 0) { \
        SelvaModify_HierarchyNode *node = SVector_Shift(&q); \
        \
        if (node_cb(node, cb->node_arg)) { \
            Trx_End(&hierarchy->trx_state, &trx_cur); \
            return 0; \
        }

#define BFS_VISIT_ADJACENT(adj_vec) do { \
        struct SVectorIterator it; \
        \
        SVector_ForeachBegin(&it, (adj_vec)); \
        SelvaModify_HierarchyNode *adj; \
        while((adj = SVector_Foreach(&it))) { \
            if (Trx_Visit(&trx_cur, &adj->trx_label)) { \
                child_cb(node, adj, cb->child_arg); \
                SVector_Insert(&q, adj); \
            } \
        } \
    } while(0)

#define BFS_TRAVERSE_END(hierarchy) \
    } \
    Trx_End(&(hierarchy)->trx_state, &trx_cur)

/**
 * BFS from a given head node towards its descendants or ancestors.
 */
static int bfs(
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *head,
        enum SelvaModify_HierarchyNode_Relationship dir,
        const TraversalCallback * restrict cb) {
    size_t offset;

    switch (dir) {
    case RELATIONSHIP_PARENT:
        offset = offsetof(SelvaModify_HierarchyNode, parents);
        break;
    case RELATIONSHIP_CHILD:
        offset = offsetof(SelvaModify_HierarchyNode, children);
        break;
    default:
        return SELVA_HIERARCHY_ENOTSUP;
    }

    BFS_TRAVERSE(hierarchy, head, cb) {
        SVector *adj_vec = (SVector *)((char *)node + offset);

        BFS_VISIT_ADJACENT(adj_vec);
    } BFS_TRAVERSE_END(hierarchy);

    return 0;
}

static int bfs_edge(
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *head,
        RedisModuleString *field_name,
        const TraversalCallback * restrict cb) {
    TO_STR(field_name);

    BFS_TRAVERSE(hierarchy, head, cb) {
        struct EdgeField *edge_field;

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
        BFS_VISIT_ADJACENT(&edge_field->arcs);
    } BFS_TRAVERSE_END(hierarchy);

    return 0;
}

static SVector *get_adj_vec(SelvaModify_HierarchyNode *node, RedisModuleString *field) {
    TO_STR(field);

    if (field_len == 8 && !strncmp("children", field_str, 8)) {
        return &node->children;
    } else if (field_len == 7 && !strncmp("parents", field_str, 7)) {
        return &node->parents;
    } else {
        /* Try EdgeField */
        struct EdgeField *edge_field;

        edge_field = Edge_GetField(node, field_str, field_len);
        if (edge_field) {
            return &edge_field->arcs;
        }
    }

    return NULL;
}

static int bfs_expression(
        RedisModuleCtx *redis_ctx,
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *head,
        struct rpn_ctx *rpn_ctx,
        struct rpn_expression *rpn_expr,
        const TraversalCallback * restrict cb) {
    struct SelvaSet fields;

    SelvaSet_Init(&fields, SELVA_SET_TYPE_RMSTRING);

    BFS_TRAVERSE(hierarchy, head, cb) {
        SVector *adj_vec;
        enum rpn_error rpn_err;
        struct SelvaSetElement *field_el;

        /* TODO support ref fields */
        rpn_set_hierarchy_node(rpn_ctx, node);
        rpn_err = rpn_selvaset(redis_ctx, rpn_ctx, rpn_expr, &fields);
        if (rpn_err) {
            fprintf(stderr, "%s:%d: RPN field selector expression failed for %.*s\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node->id);
            continue;
        }

        SELVA_SET_RMS_FOREACH(field_el, &fields) {
            RedisModuleString *field = field_el->value_rms;

            adj_vec = get_adj_vec(node, field);
            if (!adj_vec) {
                continue;
            }

            BFS_VISIT_ADJACENT(adj_vec);
        }
    } BFS_TRAVERSE_END(hierarchy);

    return 0;
}

const char *SelvaModify_HierarchyDir2str(enum SelvaModify_HierarchyTraversal dir) {
    switch (dir) {
    case SELVA_HIERARCHY_TRAVERSAL_NONE:
        return (const char *)"none";
    case SELVA_HIERARCHY_TRAVERSAL_NODE:
        return (const char *)"node";
    case SELVA_HIERARCHY_TRAVERSAL_CHILDREN:
        return (const char *)"children";
    case SELVA_HIERARCHY_TRAVERSAL_PARENTS:
        return (const char *)"parents";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
        return (const char *)"bfs_ancestors";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
        return (const char *)"bfs_descendants";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
        return (const char *)"dfs_ancestors";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
        return (const char *)"dfs_descendants";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_FULL:
        return (const char *)"dfs_full";
    case SELVA_HIERARCHY_TRAVERSAL_REF:
        return (const char *)"ref";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD:
        return (const char *)"bfs_edge_field";
    default:
        return "invalid";
    }
}

static int traverse_adjacent(
        SelvaModify_HierarchyNode *head,
        enum SelvaModify_HierarchyTraversal dir,
        const TraversalCallback *tcb) {
    SVector *adjVec;
    struct SVectorIterator it;
    SelvaModify_HierarchyNode *node;

    assert(tcb->node_cb);

    if (dir == SELVA_HIERARCHY_TRAVERSAL_CHILDREN) {
        adjVec = &head->children;
    } else if (dir == SELVA_HIERARCHY_TRAVERSAL_PARENTS) {
        adjVec = &head->parents;
    } else {
        return SELVA_HIERARCHY_EINVAL;
    }

    SVector_ForeachBegin(&it, adjVec);
    while ((node = SVector_Foreach(&it))) {
        if (tcb->node_cb(node, tcb->node_arg)) {
            return 0;
        }
    }

    return 0;
}

static int traverse_ref(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *head,
        const char *ref_field_str,
        const struct SelvaModify_HierarchyCallback *cb) {
    int err;

    RedisModuleString *head_id;
    head_id = RedisModule_CreateString(ctx, head->id, Selva_NodeIdLen(head->id));
    if (!head_id) {
        return SELVA_HIERARCHY_ENOMEM;
    }

    struct SelvaObject *head_obj;
    err = SelvaObject_Key2Obj(RedisModule_OpenKey(ctx, head_id, REDISMODULE_READ), &head_obj);
    if (err) {
        return err;
    }

    struct SelvaSet *ref_set;
    ref_set = SelvaObject_GetSetStr(head_obj, ref_field_str, strlen(ref_field_str));
    if (!ref_set) {
        return SELVA_HIERARCHY_ENOENT;
    }
    if (ref_set->type != SELVA_SET_TYPE_RMSTRING) {
        return SELVA_EINTYPE;
    }

    struct SelvaSetElement *el;
    SELVA_SET_RMS_FOREACH(el, ref_set) {
        RedisModuleString *value = el->value_rms;
        Selva_NodeId nodeId;
        SelvaModify_HierarchyNode *node;
        TO_STR(value);

        memset(nodeId, 0, SELVA_NODE_ID_SIZE);
        memcpy(nodeId, value_str, min(value_len, SELVA_NODE_ID_SIZE));

        node = SelvaHierarchy_FindNode(hierarchy, nodeId);
        if (node) {
            cb->node_cb(node, cb->node_arg);
        }
    }

    return 0;
}

// TODO
static int traverse_array(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *head,
        const char *ref_field_str,
        const struct SelvaModify_HierarchyCallback *cb) {
    // int err;

    // RedisModuleString *head_id;
    // head_id = RedisModule_CreateString(ctx, head->id, Selva_NodeIdLen(head->id));
    // if (!head_id) {
    //     return SELVA_HIERARCHY_ENOMEM;
    // }

    // struct SelvaObject *head_obj;
    // err = SelvaObject_Key2Obj(RedisModule_OpenKey(ctx, head_id, REDISMODULE_READ), &head_obj);
    // if (err) {
    //     return err;
    // }

    // struct SelvaSet *ref_set;
    // ref_set = SelvaObject_GetSetStr(head_obj, ref_field_str, strlen(ref_field_str));
    // if (!ref_set) {
    //     return SELVA_HIERARCHY_ENOENT;
    // }
    // if (ref_set->type != SELVA_SET_TYPE_RMSTRING) {
    //     return SELVA_EINTYPE;
    // }

    // struct SelvaSetElement *el;
    // SELVA_SET_RMS_FOREACH(el, ref_set) {
    //     RedisModuleString *value = el->value_rms;
    //     Selva_NodeId nodeId;
    //     SelvaModify_HierarchyNode *node;
    //     TO_STR(value);

    //     memset(nodeId, 0, SELVA_NODE_ID_SIZE);
    //     memcpy(nodeId, value_str, min(value_len, SELVA_NODE_ID_SIZE));

    //     node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    //     if (node) {
    //         cb->node_cb(node, cb->node_arg);
    //     }
    // }


    int err;

    RedisModuleString *head_id;
    head_id = RedisModule_CreateString(ctx, head->id, Selva_NodeIdLen(head->id));
    if (!head_id) {
        return SELVA_HIERARCHY_ENOMEM;
    }

    struct SelvaObject *head_obj;
    err = SelvaObject_Key2Obj(RedisModule_OpenKey(ctx, head_id, REDISMODULE_READ), &head_obj);
    if (err) {
        return err;
    }

    enum SelvaObjectType subtype;
    SVector *vec;
    err = SelvaObject_GetArrayStr(head_obj, ref_field_str, strlen(ref_field_str), &subtype, &vec);
    if (err) {
        return err;
    }

    struct SVectorIterator it;
    SVector_ForeachBegin(&it, vec);
    struct SelvaObject *obj;
    while ((obj = SVector_Foreach(&it))) {
    // TODO
    // this callback will set selva object in register 0 then field functions can operate directly on the selva object
    // and call RPN eval
    // set in result but I think it needs to work differently since we don't use ids for the find results then
    // maybe we need empty ids or nulls or something
    // we probably have a macro for empty id
    // then we don't need to make the find result have a different format for arrays
    //     if (node) {
    //         cb->node_cb(node, cb->node_arg);
    //     }
    }

    return 0;
}

/*
 * A little trampoline to hide the scary internals of the hierarchy
 * implementation from the innocent users just wanting to traverse the
 * hierarchy.
 */
static int SelvaModify_TraverseHierarchy_cb_wrapper(SelvaModify_HierarchyNode *node, void *arg) {
    struct SelvaModify_HierarchyCallback *cb = (struct SelvaModify_HierarchyCallback *)arg;

    return cb->node_cb(node, cb->node_arg);
}

int SelvaModify_TraverseHierarchy(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        enum SelvaModify_HierarchyTraversal dir,
        const struct SelvaModify_HierarchyCallback *cb) {
    const TraversalCallback tcb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = SelvaModify_TraverseHierarchy_cb_wrapper,
        .node_arg = (void *)cb,
        .child_cb = NULL,
        .child_arg = NULL,
    };
    SelvaModify_HierarchyNode *head;
    int err;

    if (dir == SELVA_HIERARCHY_TRAVERSAL_NONE) {
        return SELVA_HIERARCHY_EINVAL;
    }
    if (unlikely(dir == SELVA_HIERARCHY_TRAVERSAL_REF)) {
        /* Use SelvaModify_TraverseHierarchyRef() instead */
        return SELVA_HIERARCHY_ENOTSUP;
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
    case SELVA_HIERARCHY_TRAVERSAL_PARENTS:
        err = traverse_adjacent(head, dir, &tcb);
        break;
    case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
        err = bfs(hierarchy, head, RELATIONSHIP_PARENT, &tcb);
        break;
    case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
        err = bfs(hierarchy, head, RELATIONSHIP_CHILD, &tcb);
        break;
    case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
        err = dfs(hierarchy, head, RELATIONSHIP_PARENT, &tcb);
        break;
     case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
        err = dfs(hierarchy, head, RELATIONSHIP_CHILD, &tcb);
        break;
     case SELVA_HIERARCHY_TRAVERSAL_DFS_FULL:
        err = full_dfs(hierarchy, &tcb);
        break;
     default:
        fprintf(stderr, "%s:%d: Invalid traversal requested (%d)\n",
                __FILE__, __LINE__,
                (int)dir);
        err = SELVA_HIERARCHY_ENOTSUP;
    }

    return err;
}

int SelvaModify_TraverseHierarchyRef(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        const char *ref_field,
        const struct SelvaModify_HierarchyCallback *cb) {
    SelvaModify_HierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return traverse_ref(ctx, hierarchy, head, ref_field, cb);
}


int SelvaModify_TraverseArray(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        const char *ref_field,
        const struct SelvaModify_HierarchyCallback *cb) {
    SelvaModify_HierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return traverse_array(ctx, hierarchy, head, ref_field, cb);
}

int SelvaModify_TraverseHierarchyEdge(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        RedisModuleString *field_name,
        const struct SelvaModify_HierarchyCallback *cb) {
    const TraversalCallback tcb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = SelvaModify_TraverseHierarchy_cb_wrapper,
        .node_arg = (void *)cb,
        .child_cb = NULL,
        .child_arg = NULL,
    };
    SelvaModify_HierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return bfs_edge(hierarchy, head, field_name, &tcb);
}

int SelvaHierarchy_TraverseExpression(
        struct RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        struct rpn_ctx *rpn_ctx,
        struct rpn_expression *rpn_expr,
        const struct SelvaModify_HierarchyCallback *cb) {
    const TraversalCallback tcb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = SelvaModify_TraverseHierarchy_cb_wrapper,
        .node_arg = (void *)cb,
        .child_cb = NULL,
        .child_arg = NULL,
    };
    SelvaModify_HierarchyNode *head;

    head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    return bfs_expression(ctx, hierarchy, head, rpn_ctx, rpn_expr, &tcb);
}

static int dfs_make_list_node_cb(SelvaModify_HierarchyNode *node, void *arg) {
    void **args = (void **)arg;
    SelvaModify_HierarchyNode *head = (SelvaModify_HierarchyNode *)args[0];
    ssize_t *nr_nodes = (ssize_t *)args[1];
    Selva_NodeId **list = (Selva_NodeId **)args[2];

    if (*list && node != head) {
        *nr_nodes = *nr_nodes + 1;
        *list = NodeList_Insert(*list, node->id, *nr_nodes);
        if (unlikely(!(*list))) {
            *nr_nodes = SELVA_HIERARCHY_ENOMEM;
        }
    }

    return 0;
}

static ssize_t SelvaModify_FindDir(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, enum SelvaModify_HierarchyNode_Relationship dir, Selva_NodeId **res) {
    SelvaModify_HierarchyNode *head = SelvaHierarchy_FindNode(hierarchy, id);
    if (!head) {
        return SELVA_HIERARCHY_ENOENT;
    }

    int err;
    ssize_t nr_nodes = 0;
    Selva_NodeId *list = NodeList_New(1);
    void *args[] = { head, &nr_nodes, &list };
    const TraversalCallback cb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = dfs_make_list_node_cb,
        .node_arg = args,
        .child_cb = NULL,
        .child_arg = NULL,
    };

    err = dfs(hierarchy, head, dir, &cb);
    if (err != 0) {
        *res = NULL;
        RedisModule_Free(list);

        return err;
    } else if (nr_nodes <= 0) {
        *res = NULL;
        RedisModule_Free(list);
    } else {
        *res = list;
    }

    return nr_nodes;
}

ssize_t SelvaModify_FindAncestors(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **ancestors) {
    return SelvaModify_FindDir(hierarchy, id, RELATIONSHIP_PARENT, ancestors);
}

ssize_t SelvaModify_FindDescendants(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **descendants) {
    return SelvaModify_FindDir(hierarchy, id, RELATIONSHIP_CHILD, descendants);
}

int SelvaHierarchy_IsNonEmptyField(struct SelvaModify_HierarchyNode *node, const char *field_str, size_t field_len) {

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

int load_metadata(RedisModuleIO *io, int encver, SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node) {
    if (unlikely(!node)) {
        return SELVA_EINVAL;
    }

    /*
     * Note that the metadata must be loaded and saved in predefined order.
     */
    return Edge_RdbLoad(io, encver, hierarchy, node);
}

void *HierarchyTypeRDBLoad(RedisModuleIO *io, int encver) {
    if (encver > HIERARCHY_ENCODING_VERSION) {
        RedisModule_LogIOError(io, "warning", "Unknown selva_hierarchy version");
        return NULL;
    }

#if HIERARCHY_SORT_BY_DEPTH
    rdbLoading = 1;
#endif
    SelvaModify_Hierarchy *hierarchy = SelvaModify_NewHierarchy(NULL);

    while (1) {
        int err;
        size_t len = 0;
        char *node_id __attribute__((cleanup(wrapFree))) = NULL;

        node_id = RedisModule_LoadStringBuffer(io, &len);
        if (!node_id || len != SELVA_NODE_ID_SIZE) {
             RedisModule_LogIOError(io, "warning", "Failed to load the next nodeId");
            goto error;
        }

        /*
         * If it's EOF there are no more nodes for this hierarchy.
         */
        if (!memcmp(node_id, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE)) {
            break;
        }

        /*
         * The node metadata comes right after the node_id.
         */
        SelvaModify_HierarchyNode *node;
        err = SelvaHierarchy_UpsertNode(RedisModule_GetContextFromIO(io), hierarchy, node_id, &node);
        if (err && err != SELVA_HIERARCHY_EEXIST) {
            RedisModule_LogIOError(io, "warning", "Failed to upsert %.*s: %s",
                                   (int)SELVA_NODE_ID_SIZE, node_id,
                                   getSelvaErrorStr(err));
            goto error;
        }
        err = load_metadata(io, encver, hierarchy, node);
        if (err) {
            RedisModule_LogIOError(io, "warning", "Failed to load hierarchy metadata");
            goto error;
        }

        uint64_t nr_children = RedisModule_LoadUnsigned(io);
        Selva_NodeId *children __attribute__((cleanup(wrapFree))) = NULL;

        if (nr_children > 0) {
            children = RedisModule_Calloc(nr_children, SELVA_NODE_ID_SIZE);
            if (!children) {
                goto error;
            }

            /* Create/Update children */
            for (uint64_t i = 0; i < nr_children; i++) {
                char *child_id __attribute__((cleanup(wrapFree))) = NULL;

                child_id = RedisModule_LoadStringBuffer(io, &len);
                if (len != SELVA_NODE_ID_SIZE) {
                    goto error;
                }

                err = SelvaModify_AddHierarchy(NULL, hierarchy, child_id, 0, NULL, 0, NULL);
                if (err < 0) {
                    RedisModule_LogIOError(io, "warning", "Unable to rebuild the hierarchy");
                    goto error;
                }

                memcpy(children + i, child_id, SELVA_NODE_ID_SIZE);
            }
        }

        /*
         * Insert children of the node.
         * TODO We could make this faster by skipping the lookup as we already
         * have a pointer to the node.
         */
        err = SelvaModify_AddHierarchy(NULL, hierarchy, node_id, 0, NULL, nr_children, children);
        if (err < 0) {
            RedisModule_LogIOError(io, "warning", "Unable to rebuild the hierarchy");
            goto error;
        }
    }

#if HIERARCHY_SORT_BY_DEPTH
    rdbLoading = 0;

    /*
     * Update depths on a single pass to save time.
     */
    struct SVectorIterator it;
    SelvaModify_HierarchyNode *head;

    SVector_ForeachBegin(&it, &hierarchy->heads);
    while ((head = SVector_Foreach(&it))) {
        updateDepth(hierarchy, head);
    }
#endif

    return hierarchy;
error:
    SelvaModify_DestroyHierarchy(hierarchy);

    return NULL;
}

void save_metadata(RedisModuleIO *io, SelvaModify_HierarchyNode *node) {
    /*
     * Note that the metadata must be loaded and saved in predefined order.
     */

    Edge_RdbSave(io, node);
}

static int HierarchyRDBSaveNode(SelvaModify_HierarchyNode *node, void *arg) {
    RedisModuleIO *io = (RedisModuleIO *)arg;

    RedisModule_SaveStringBuffer(io, node->id, SELVA_NODE_ID_SIZE);
    save_metadata(io, node);
    RedisModule_SaveUnsigned(io, SVector_Size(&node->children));

    return 0;
}

static void HierarchyRDBSaveChild(SelvaModify_HierarchyNode *parent, SelvaModify_HierarchyNode *child, void *arg) {
    REDISMODULE_NOT_USED(parent);
    RedisModuleIO *io = (RedisModuleIO *)arg;

    RedisModule_SaveStringBuffer(io, child->id, SELVA_NODE_ID_SIZE);
}

void HierarchyTypeRDBSave(RedisModuleIO *io, void *value) {
    SelvaModify_Hierarchy *hierarchy = (SelvaModify_Hierarchy *)value;
    const TraversalCallback cb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = HierarchyRDBSaveNode,
        .node_arg = io,
        .child_cb = HierarchyRDBSaveChild,
        .child_arg = io,
    };

    /*
     * Serialization format:
     * NODE_ID1 | METADATA | NR_CHILDREN | CHILD_ID_0,..
     * NODE_ID2 | METADATA | NR_CHILDREN | ...
     * HIERARCHY_RDB_EOF
     */
    (void)full_dfs(hierarchy, &cb);
    RedisModule_SaveStringBuffer(io, HIERARCHY_RDB_EOF, sizeof(HIERARCHY_RDB_EOF));
}

static void HierarchyAOFSaveHead(SelvaModify_HierarchyNode *node, void *arg) {
    void **args = (void **)arg;
    RedisModuleIO *aof = (RedisModuleIO *)args[0];
    RedisModuleString *key = (RedisModuleString *)args[1];

    /* Create the head node */
    RedisModule_EmitAOF(aof, "SELVA.HIERARCHY.ADD", "sb",
        key,
        node->id, (size_t)SELVA_NODE_ID_SIZE);
}

static void HierarchyAOFSaveChild(SelvaModify_HierarchyNode *parent, SelvaModify_HierarchyNode *child, void *arg) {
    void **args = (void **)arg;
    RedisModuleIO *aof = (RedisModuleIO *)args[0];
    RedisModuleString *key = (RedisModuleString *)args[1];

    /* Create the children */
    RedisModule_EmitAOF(aof, "SELVA.HIERARCHY.ADD", "sbb",
    key,
    child->id, (size_t)SELVA_NODE_ID_SIZE,
    parent->id, (size_t)SELVA_NODE_ID_SIZE);
}

void HierarchyTypeAOFRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    SelvaModify_Hierarchy *hierarchy = (SelvaModify_Hierarchy *)value;
    void *args[] = { aof, key };
    const TraversalCallback cb = {
        .head_cb = HierarchyAOFSaveHead,
        .head_arg = args,
        .node_cb = NULL,
        .node_arg = NULL,
        .child_cb = HierarchyAOFSaveChild,
        .child_arg = args,
    };

    (void)full_dfs(hierarchy, &cb);
}

void HierarchyTypeFree(void *value) {
    SelvaModify_Hierarchy *hierarchy = (SelvaModify_Hierarchy *)value;

    SelvaModify_DestroyHierarchy(hierarchy);
}

/*
 * SELVA.HIERARCHY.DEL HIERARCHY_KEY [NODE_ID1[, NODE_ID2, ...]]
 * If no NODE_IDs are given then nothing will be deleted.
 */
int SelvaModify_Hierarchy_DelNodeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 2) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    long long nr_deleted = 0;
    for (int i = 2; i < argc; i++) {
        Selva_NodeId nodeId;

        RMString2NodeId(nodeId, argv[i]);

        if (!SelvaModify_DelHierarchyNode(ctx, hierarchy, nodeId)) {
            nr_deleted++;
        }
    }

    RedisModule_ReplyWithLongLong(ctx, nr_deleted);
    RedisModule_ReplicateVerbatim(ctx);
    SelvaSubscriptions_SendDeferredEvents(hierarchy);

    return REDISMODULE_OK;
}

/*
 * SELVA.HIERARCHY.DELREF HIERARCHY_KEY NODE_ID PARENTS|CHILDREN
 */
int SelvaModify_Hierarchy_DelRefCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    Selva_NodeId nodeId;

    RMString2NodeId(nodeId, argv[2]);

    SelvaModify_HierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    const char *op = RedisModule_StringPtrLen(argv[3], NULL);
    if (!strcmp(op, "parents")) {
        removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);

        /*
         * Reparent to root if the node is now orphan.
         */
        if (SVector_Size(&node->parents) == 0) {
            (void)SelvaModify_SetHierarchy(ctx, hierarchy, node->id,
                1, ((Selva_NodeId []){ ROOT_NODE_ID }),
                0, NULL);
        }
    } else if (!strcmp(op, "children")) {
        Selva_NodeId *ids;
        size_t ids_len;

        /* RFE Shouldn't this come later? */
        removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);

        ids = getNodeIds(&node->children, &ids_len);
        if (unlikely(!ids)) {
            return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOMEM);
        }

        for (size_t i = 0; i < ids_len; i++) {
            Selva_NodeId nodeId;

            memcpy(nodeId, ids + i, SELVA_NODE_ID_SIZE);

            /*
             * Find the node.
             */
            SelvaModify_HierarchyNode *child = SelvaHierarchy_FindNode(hierarchy, nodeId);
            if (!child) {
                /* Node not found;
                 * This is probably fine, as there might have been a circular link.
                 */
                continue;
            }

            if (SVector_Size(&child->parents) == 0) {
                int err;
                Selva_NodeId childId;

                /*
                 * Make a copy of the child's ID just for the sake of
                 * potentially logging it.
                 */
                memcpy(childId, child->id, SELVA_NODE_ID_SIZE);

                err = SelvaModify_DelHierarchyNodeP(ctx, hierarchy, child);
                if (err) {
                    /*
                     * We ignore and log any errors.
                     */
                    fprintf(stderr, "%s:%d: Failed to delete the child \"%.*s\" of \"%.*s\"\n",
                            __FILE__, __LINE__,
                            (int)SELVA_NODE_ID_SIZE, childId,
                            (int)SELVA_NODE_ID_SIZE, nodeId);
                }
            }
        }
    } else {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOTSUP);
    }

    RedisModule_ReplyWithLongLong(ctx, 1);
    RedisModule_ReplicateVerbatim(ctx);
    SelvaSubscriptions_SendDeferredEvents(hierarchy);

    return REDISMODULE_OK;
}

int SelvaModify_Hierarchy_ParentsCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    RMString2NodeId(nodeId, argv[2]);
    SelvaModify_HierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
    }

    struct SVectorIterator it;
    SelvaModify_HierarchyNode *parent;
    SVector *parents;

#if HIERARCHY_SORT_BY_DEPTH
    SVECTOR_AUTOFREE(Sparents_d);

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

int SelvaModify_Hierarchy_ChildrenCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    RMString2NodeId(nodeId, argv[2]);
    SelvaModify_HierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
    }

    RedisModule_ReplyWithArray(ctx, SVector_Size(&node->children));

    struct SVectorIterator it;
    SelvaModify_HierarchyNode *child;

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
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    RMString2NodeId(nodeId, argv[2]);
    SelvaModify_HierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
    }

    struct SelvaObject *obj = node->metadata.edge_fields.edges;

    if (!obj) {
        /* No custom edges set. */
        return RedisModule_ReplyWithArray(ctx, 0);
    }

    RedisModuleString *key_name = argc == 4 ? argv[3] : NULL;
    if (key_name) {
        int err;
        err = SelvaObject_GetObject(obj, key_name, &obj);
        if (err) {
            return replyWithSelvaError(ctx, err);
        }
    }

    SelvaObject_ReplyWithObject(ctx, NULL, obj, NULL);

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
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    RMString2NodeId(nodeId, argv[2]);
    SelvaModify_HierarchyNode *node = SelvaHierarchy_FindNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_HIERARCHY_ENOENT);
    }

    struct SelvaObject *obj = node->metadata.edge_fields.edges;
    if (!obj) {
        /* No custom edges set. */
        return RedisModule_ReplyWithNull(ctx);
    }

    RedisModuleString *key_name = argv[3];
    struct EdgeField *edge_field;
    int err;

    err = SelvaObject_GetPointer(obj, key_name, (void **)(&edge_field));
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    struct SVector *arcs = &edge_field->arcs;
    struct SVectorIterator it;
    SelvaModify_HierarchyNode *dst;

    RedisModule_ReplyWithArray(ctx, 1 + SVector_Size(arcs));
    RedisModule_ReplyWithLongLong(ctx, edge_field->constraint_id);

    SVector_ForeachBegin(&it, arcs);
    while ((dst = SVector_Foreach(&it))) {
        RedisModule_ReplyWithStringBuffer(ctx, dst->id, Selva_NodeIdLen(dst->id));
    }

    return REDISMODULE_OK;
}

static int Hierarchy_OnLoad(RedisModuleCtx *ctx) {
    RedisModuleTypeMethods tm = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = HierarchyTypeRDBLoad,
        .rdb_save = HierarchyTypeRDBSave,
        .aof_rewrite = HierarchyTypeAOFRewrite,
        .free = HierarchyTypeFree,
    };

    HierarchyType = RedisModule_CreateDataType(ctx, "hierarchy", HIERARCHY_ENCODING_VERSION, &tm);
    if (HierarchyType == NULL) {
        return REDISMODULE_ERR;
    }

    /*
     * Register commands.
     */
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.del", SelvaModify_Hierarchy_DelNodeCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.delref", SelvaModify_Hierarchy_DelRefCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.parents", SelvaModify_Hierarchy_ParentsCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.children", SelvaModify_Hierarchy_ChildrenCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.edgelist", SelvaHierarchy_EdgeListCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.edgeget", SelvaHierarchy_EdgeGetCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Hierarchy_OnLoad);
