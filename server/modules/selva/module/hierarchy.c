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
#include "modify.h"
#include "rpn.h"
#include "selva_node.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "subscriptions.h"
#include "svector.h"

#define HIERARCHY_ENCODING_VERSION  0

typedef struct SelvaModify_HierarchyNode {
    Selva_NodeId id;
    struct timespec visit_stamp;
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

/*!<
 * DB is loading.
 * If set then some expensive operations can be skipped and/or deferred.
 */
static int rdbLoading;

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
    SelvaModify_Hierarchy *hierarchy = RedisModule_Calloc(1, sizeof(SelvaModify_HierarchyNode));
    if (unlikely(!hierarchy)) {
        goto fail;
    }

    RB_INIT(&hierarchy->index_head);
    if (unlikely(!SVector_Init(&hierarchy->heads, 1, SVector_HierarchyNode_id_compare))) {
        RedisModule_Free(hierarchy);
        hierarchy = NULL;
        goto fail;
    }

    /*
     * Subscriptions.
     */
    RB_INIT(&hierarchy->subs.head);
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

    if(unlikely(SelvaModify_SetHierarchy(ctx, hierarchy, ROOT_NODE_ID, 0, NULL, 0, NULL))) {
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
                replyWithSelvaError(ctx, SELVA_MODIFY_HIERARCHY_ENOMEM);
            }

            RedisModule_ModuleTypeSetValue(key, HierarchyType, hierarchy);
        } else {
            replyWithSelvaError(ctx, SELVA_MODIFY_HIERARCHY_ENOENT);
        }
    } else {
        hierarchy = RedisModule_ModuleTypeGetValue(key);
        if (!hierarchy) {
            replyWithSelvaError(ctx, SELVA_MODIFY_HIERARCHY_ENOENT);
        }
    }

    return hierarchy;
}

static int createNodeHash(RedisModuleCtx *ctx, const Selva_NodeId id) {
    RedisModuleString *node_name;
    RedisModuleKey *key = NULL;

    node_name = RedisModule_CreateStringPrintf(ctx, "%.*s", SELVA_NODE_ID_SIZE, id);
    if (unlikely(!node_name)) {
        return SELVA_MODIFY_HIERARCHY_ENOMEM;
    }

    /*
     * Open the key to create the mandatory fields used internally.
     */
    key = SelvaNode_Open(ctx, NULL, node_name, id, SELVA_NODE_OPEN_WRFLD_FLAG);
    if (!key) {
        return SELVA_MODIFY_HIERARCHY_ENOMEM;
    }

    /* We don't actually need to access it so just close it immediately. */
    RedisModule_CloseKey(key);

    return 0;
}

static SelvaModify_HierarchyNode *newNode(RedisModuleCtx *ctx, const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node = RedisModule_Calloc(1, sizeof(SelvaModify_HierarchyNode));
    if (unlikely(!node)) {
        return NULL;
    };

    if (unlikely(!SVector_Init(&node->parents, HIERARCHY_INITIAL_VECTOR_LEN, SVector_HierarchyNode_id_compare) ||
                 !SVector_Init(&node->children, HIERARCHY_INITIAL_VECTOR_LEN, SVector_HierarchyNode_id_compare))) {
        SelvaModify_DestroyNode(node);
        return NULL;
    }

    memcpy(node->id, id, SELVA_NODE_ID_SIZE);

    if (likely(ctx)) {
        int err;

        err = createNodeHash(ctx, id);
        if (err) {
            fprintf(stderr, "%s: Failed to create a hash for \"%.*s\": %s\n",
                    __FILE__,
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
    RedisModule_Free(node);
}

static char *get_node_field_names(RedisModuleCtx *ctx, Selva_NodeId id) {
    RedisModuleCallReply * reply;
    char *res = NULL;
    size_t res_len = 0;

    reply = RedisModule_Call(ctx, "HKEYS", "b", id, SELVA_NODE_ID_SIZE);
    if (reply == NULL) {
        /* FIXME errno handling */
#if 0
        switch (errno) {
        case EINVAL:
        case EPERM:
        default:
        }
#endif
        goto hkeys_err;
    }

    int replyType = RedisModule_CallReplyType(reply);
    if (replyType != REDISMODULE_REPLY_ARRAY) {
        goto hkeys_err;
    }

    size_t replyLen = RedisModule_CallReplyLength(reply);
    for (size_t idx = 0; idx < replyLen; idx++) {
        RedisModuleCallReply *elem;
        const char * field_str;
        size_t field_len;

        elem = RedisModule_CallReplyArrayElement(reply, idx);
        if (!elem) {
            continue;
        }

        int elemType = RedisModule_CallReplyType(elem);
        if (elemType != REDISMODULE_REPLY_STRING) {
            continue;
        }

        field_str = RedisModule_CallReplyStringPtr(elem, &field_len);
        if (!field_str) {
            continue;
        }

        /*
         * Append the res string.
         */
        res_len += field_len + 1;
        res = RedisModule_Realloc(res, res_len);
        memcpy(res + res_len - field_len - 1, field_str, field_len);
        res[res_len - 1] = ',';
    }

hkeys_err:
    if (reply) {
        RedisModule_FreeCallReply(reply);
    }
    if (res) {
        res[res_len - 1] = '\0';
    }

    return res;
}

static SelvaModify_HierarchyNode *findNode(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
        SelvaModify_HierarchySearchFilter filter;

        memcpy(&filter.id, id, SELVA_NODE_ID_SIZE);
        return RB_FIND(hierarchy_index_tree, &hierarchy->index_head, (SelvaModify_HierarchyNode *)(&filter));
}

int SelvaModify_HierarchyNodeExists(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
    return findNode(hierarchy, id) != NULL;
}

struct SelvaModify_HierarchyMetadata *SelvaModify_HierarchyGetNodeMetadata(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node;

    node = findNode(hierarchy, id);
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

    removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);

    /*
     * Never delete the root node.
     */
    if (!is_root) {
        removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);

        /*
         * The node was now marked as a head but we are going to get rid of it
         * soon, so there is no reason to make it a tree head.
         */
        rmHead(hierarchy, node);

        RB_REMOVE(hierarchy_index_tree, &hierarchy->index_head, node);
        SelvaModify_DestroyNode(node);
        node = NULL;
    }

    if (likely(ctx)) {
        RedisModuleString *rms_id;

        rms_id = RedisModule_CreateString(ctx, id, Selva_NodeIdLen(id));
        /* TODO We may want a deletion marker support? */
#if 0
        char *fields;

        fields = get_node_field_names(ctx, id);
        SelvaNode_Delete(ctx, rms_id);
        /* event here */
        RedisModule_Free(fields);
#endif
        SelvaNode_Delete(ctx, rms_id);

        if (is_root) {
            createNodeHash(ctx, id);
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
        fprintf(stderr, "%s: Depth update error\n", __FILE__);
        abort();
    }

    Trx_Begin(&hierarchy->current_trx);
    Trx_Stamp(&hierarchy->current_trx, &head->visit_stamp);
    (void)SVector_InsertFast(&q, head);

    while (SVector_Size(&q) > 0) {
        SelvaModify_HierarchyNode *node = SVector_Shift(&q);
        SelvaModify_HierarchyNode **child_pp;

        /*
         * Update the depth.
         */
        ssize_t new_depth = 0;
        SelvaModify_HierarchyNode **parent_pp;
        SVECTOR_FOREACH(parent_pp, &node->parents) {
            SelvaModify_HierarchyNode *parent = *parent_pp;

            new_depth = max(new_depth, parent->depth + 1);
        }
        node->depth = new_depth;

        SVECTOR_FOREACH(child_pp, &node->children) {
            SelvaModify_HierarchyNode *child = *child_pp;

            if (!Trx_IsStamped(&hierarchy->current_trx, &child->visit_stamp)) {
                Trx_Stamp(&hierarchy->current_trx, &child->visit_stamp);
                SVector_Insert(&q, child);
            }
        }
    }

    SVector_Destroy(&q);
}
#endif

#if HIERARCHY_SORT_BY_DEPTH
ssize_t SelvaModify_GetHierarchyDepth(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
    const SelvaModify_HierarchyNode *node;

    node = findNode(hierarchy, id);
    if (!node) {
        return -1;
    }

    return node->depth;
}
#endif

static inline void publishAncestorsUpdate(struct SelvaModify_Hierarchy *hierarchy, const SelvaModify_HierarchyNode *node) {
    SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node->id, &node->metadata, "ancestors");
}

static inline void publishDescendantsUpdate(struct SelvaModify_Hierarchy *hierarchy, const SelvaModify_HierarchyNode *node) {
    SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node->id, &node->metadata, "descendants");
}

static inline void publishChildrenUpdate(struct SelvaModify_Hierarchy *hierarchy, const SelvaModify_HierarchyNode *node) {
    SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node->id, &node->metadata, "children");
}

static inline void publishParentsUpdate(struct SelvaModify_Hierarchy *hierarchy, const SelvaModify_HierarchyNode *node) {
    SelvaSubscriptions_DeferFieldChangeEvents(hierarchy, node->id, &node->metadata, "parents");
}

static int crossInsert(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        SelvaModify_HierarchyNode *node,
        enum SelvaModify_HierarchyNode_Relationship rel,
        size_t n,
        const Selva_NodeId *nodes) {
    const size_t initialNodeParentsSize = SVector_Size(&node->parents);
    int err = 0;

    if (n == 0) {
        return 0; /* No changes. */
    }

    if (rel == RELATIONSHIP_CHILD && initialNodeParentsSize == 0) {
        /* The node is no longer an orphan */
        rmHead(hierarchy, node);
    }

    SelvaSubscriptions_DeferHierarchyEvents(hierarchy, node->id, &node->metadata);
    if (rel == RELATIONSHIP_CHILD) { /* node is a child to adjacent */
        for (size_t i = 0; i < n; i++) {
            SelvaModify_HierarchyNode *adjacent = findNode(hierarchy, nodes[i]);

            if (!adjacent) {
                /* RFE no_root is not propagated */
                err = SelvaModify_SetHierarchy(ctx, hierarchy, nodes[i],
                        1, ((Selva_NodeId []){ ROOT_NODE_ID }),
                        0, NULL);
                if (err) {
                    fprintf(stderr, "%s: Failed to create a parent \"%.*s\" for \"%.*s\": %s\n",
                            __FILE__,
                            (int)SELVA_NODE_ID_SIZE, nodes[i],
                            (int)SELVA_NODE_ID_SIZE, node->id,
                            selvaStrError[-err]);
                    continue;
                }

                adjacent = findNode(hierarchy, nodes[i]);
                if (!adjacent) {
                    fprintf(stderr, "%s: Node state error, node: \"%.*s\"\n",
                            __FILE__,
                            (int)SELVA_NODE_ID_SIZE, nodes[i]);
                    return SELVA_MODIFY_HIERARCHY_EGENERAL;
                }
            }

            /* Do inserts only if the relationship doesn't exist already */
            if (SVector_InsertFast(&node->parents, adjacent) == NULL) {
                (void)SVector_InsertFast(&adjacent->children, node);

                /*
                 * Publish that the children field was changed.
                 * Actual events are only sent if there are subscription markers
                 * set on this node.
                 */
                publishChildrenUpdate(hierarchy, adjacent);
                publishDescendantsUpdate(hierarchy, adjacent);
                SelvaSubscriptions_InheritParent(
                    hierarchy,
                    node->id, &node->metadata,
                    SVector_Size(&node->children),
                    adjacent->id, &adjacent->metadata);
            }
        }

        /*
         * Publish that the parents field was changed.
         */
        publishParentsUpdate(hierarchy, node);
        publishAncestorsUpdate(hierarchy, node);
    } else if (rel == RELATIONSHIP_PARENT) { /* node is a parent to adjacent */
        for (size_t i = 0; i < n; i++) {
            SelvaModify_HierarchyNode *adjacent = findNode(hierarchy, nodes[i]);

            if (!adjacent) {
                err = SelvaModify_SetHierarchy(ctx, hierarchy, nodes[i],
                        0, NULL,
                        0, NULL);
                if (err) {
                    fprintf(stderr, "%s: Failed to create a child \"%.*s\" for \"%.*s\": %s\n",
                            __FILE__,
                            (int)SELVA_NODE_ID_SIZE, nodes[i],
                            (int)SELVA_NODE_ID_SIZE, node->id,
                            selvaStrError[-err]);
                    continue;
                }

                adjacent = findNode(hierarchy, nodes[i]);
                if (!adjacent) {
                    fprintf(stderr, "%s: Node state error, node: \"%.*s\"\n",
                            __FILE__, (int)SELVA_NODE_ID_SIZE, nodes[i]);
                    return SELVA_MODIFY_HIERARCHY_EGENERAL;
                }
            }

            /* The adjacent node is no longer an orphan */
            if (SVector_Size(&adjacent->parents) == 0) {
                rmHead(hierarchy, adjacent);
            }

            if (SVector_InsertFast(&node->children, adjacent) == NULL) {
                (void)SVector_InsertFast(&adjacent->parents, node);

                /*
                 * Publish that the parents field was changed.
                 * Actual events are only sent if there are subscription markers
                 * set on this node.
                 */
                publishParentsUpdate(hierarchy, adjacent);
                publishAncestorsUpdate(hierarchy, adjacent);
                SelvaSubscriptions_InheritChild(
                    hierarchy,
                    node->id, &node->metadata,
                    SVector_Size(&node->parents),
                    adjacent->id, &adjacent->metadata);
            }

            publishChildrenUpdate(hierarchy, node);
            publishDescendantsUpdate(hierarchy, node);
        }

        /*
         * Publish that the children field was changed.
         */
        publishChildrenUpdate(hierarchy, node);
        publishDescendantsUpdate(hierarchy, node);
    } else {
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
    }

#if HIERARCHY_SORT_BY_DEPTH
    updateDepth(hierarchy, node);
#endif

    return err;
}

static int crossRemove(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node, enum SelvaModify_HierarchyNode_Relationship rel, size_t n, const Selva_NodeId *nodes) {
    svector_autofree SVector sub_markers;

    /*
     * Backup the subscription markers so we can refresh them after the
     * operation.
     */
    if (unlikely(!SVector_Clone(&sub_markers, &node->metadata.sub_markers.vec, NULL))) {
        return SELVA_MODIFY_HIERARCHY_ENOMEM;
    }

    SelvaSubscriptions_DeferHierarchyEvents(hierarchy, node->id, &node->metadata);
    SelvaSubscriptions_ClearAllMarkers(hierarchy, node->id, &node->metadata);

    if (rel == RELATIONSHIP_CHILD) { /* no longer a child of adjacent */
        const size_t initialNodeParentsSize = SVector_Size(&node->parents);
        int pubParents = 0;

        for (size_t i = 0; i < n; i++) {
            SelvaModify_HierarchyNode *adjacent = findNode(hierarchy, nodes[i]);

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
            publishChildrenUpdate(hierarchy, adjacent);
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
            SelvaModify_HierarchyNode *adjacent = findNode(hierarchy, nodes[i]);

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
            publishParentsUpdate(hierarchy, adjacent);
            pubChildren = 1;
        }

        if (pubChildren) {
            publishChildrenUpdate(hierarchy, node);
        }
    } else {
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
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
    svector_autofree SVector sub_markers = {0};

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
    if (unlikely(!SVector_Clone(&sub_markers, &node->metadata.sub_markers.vec, NULL))) {
        fprintf(stderr, "%s: %s ENOMEM\n", __FILE__, __func__);
        return;
    }

    SelvaSubscriptions_DeferHierarchyEvents(hierarchy, node->id, &node->metadata);
    SelvaSubscriptions_ClearAllMarkers(hierarchy, node->id, &node->metadata);

    SelvaModify_HierarchyNode **itt;
    SVector *vec_a = (SVector *)((char *)node + offset_a);

    SVECTOR_FOREACH(itt, vec_a) {
        SelvaModify_HierarchyNode *it = *itt;
        SVector *vec_b = (SVector *)((char *)it + offset_b);

        SVector_Remove(vec_b, node);

        if (rel == RELATIONSHIP_PARENT && SVector_Size(vec_b) == 0) {
            /* This node is now orphan */
            mkHead(hierarchy, it);
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
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);

    if (!node) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }

    removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);

    return 0;
}

int SelvaModify_DelHierarchyParents(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);

    if (!node) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
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
    int err;
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);
    int isNewNode = 0;

    if (!node) {
        node = newNode(ctx, id);
        if (unlikely(!node)) {
            return SELVA_MODIFY_HIERARCHY_ENOMEM;
        }
        isNewNode = 1;
    }

    if (isNewNode) {
        if (unlikely(RB_INSERT(hierarchy_index_tree, &hierarchy->index_head, node) != NULL)) {
            SelvaModify_DestroyNode(node);

            return SELVA_MODIFY_HIERARCHY_EEXIST;
        }
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
    err = crossInsert(ctx, hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents);
    if (err) {
        if (isNewNode) {
            del_node(ctx, hierarchy, node);
        }
        return err;
    }
    err = crossInsert(ctx, hierarchy, node, RELATIONSHIP_PARENT, nr_children, children);
    if (err) {
        if (isNewNode) {
            del_node(ctx, hierarchy, node);
        }
        return err;
    }

    return 0;
}

int SelvaModify_SetHierarchyParents(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents) {
    int err;
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);

    if (!node) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }

    /* Clear the existing node relationships */
    removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);

    if (nr_parents == 0) {
        /* This node is orphan */
        mkHead(hierarchy, node);
    }

    /*
     * Set relationship relative to other nodes
     */
    err = crossInsert(ctx, hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents);
    if (err) {
        return err;
    }

    return 0;
}

int SelvaModify_SetHierarchyChildren(
        RedisModuleCtx *ctx,
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_children,
        const Selva_NodeId *children) {
    int err;
    SelvaModify_HierarchyNode *node;

    node = findNode(hierarchy, id);
    if (!node) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }

    /* Clear the existing node relationships */
    removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);

    /*
     * Set relationship relative to other nodes
     */
    err = crossInsert(ctx, hierarchy, node, RELATIONSHIP_PARENT, nr_children, children);
    if (err) {
        return err;
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
    int err;
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);
    int isNewNode = 0;

    if (!node) {
        node = newNode(ctx, id);
        if (unlikely(!node)) {
            return SELVA_MODIFY_HIERARCHY_ENOMEM;
        }
        isNewNode = 1;
    }

    if (isNewNode) {
        if (RB_INSERT(hierarchy_index_tree, &hierarchy->index_head, node) != NULL) {
            SelvaModify_DestroyNode(node);

            return SELVA_MODIFY_HIERARCHY_EEXIST;
        }

        if (nr_parents == 0) {
            /* This node is orphan */
            mkHead(hierarchy, node);
        }
    }

    /*
     * Update relationship relative to other nodes
     * RFE if isNewNode == 0 then errors are not handled properly as
     * we don't know how to rollback.
     */
    err = crossInsert(ctx, hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents);
    if (err && isNewNode) {
        del_node(ctx, hierarchy, node);
        return err;
    }
    err = crossInsert(ctx, hierarchy, node, RELATIONSHIP_PARENT, nr_children, children);
    if (err && isNewNode) {
        del_node(ctx, hierarchy, node);
        return err;
    }

    return 0;
}

int SelvaModify_DelHierarchy(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);
    int err1, err2;

    if (!node) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }

    err1 = crossRemove(hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents);
    err2 = crossRemove(hierarchy, node, RELATIONSHIP_PARENT, nr_children, children);

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
    SelvaModify_HierarchyNode **adj_pp;

    ids = NodeList_New(SVector_Size(adjacent_nodes));
    if (!ids) {
        return NULL;
    }

    SVECTOR_FOREACH(adj_pp, adjacent_nodes) {
        SelvaModify_HierarchyNode *adj = *adj_pp;

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
    size_t ids_len;

    assert(("hierarchy must be set", hierarchy));
    assert(("node must be set", node));

    ids = getNodeIds(&node->children, &ids_len);
    if (unlikely(!ids)) {
        return SELVA_MODIFY_HIERARCHY_ENOMEM;
    }

    /*
     * Delete orphan children recursively.
     */
    for (size_t i = 0; i < ids_len; i++) {
        Selva_NodeId nodeId;
        int err;

        memcpy(nodeId, ids + i, SELVA_NODE_ID_SIZE);

        /*
         * Find the node.
         */
        SelvaModify_HierarchyNode *child = findNode(hierarchy, nodeId);
        if (!child) {
            /* Node not found;
             * This is probably fine, as there might have been a circular link.
             */
            continue;
        }

        err = crossRemove(hierarchy, node, RELATIONSHIP_PARENT, 1, (Selva_NodeId *)child->id);
        if (err) {
            return err;
        }

        /*
         * Recursively delete children if this node no longer has parents.
         */
        if (SVector_Size(&child->parents) == 0) {
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
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);

    if (!node) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }

    return SelvaModify_DelHierarchyNodeP(ctx, hierarchy, node);
}

ssize_t SelvaModify_GetHierarchyHeads(SelvaModify_Hierarchy *hierarchy, Selva_NodeId **res) {
    SelvaModify_HierarchyNode **it;
    Selva_NodeId *list = NodeList_New(1);
    ssize_t nr_nodes = 0;

    SVECTOR_FOREACH(it, &hierarchy->heads) {
        list = NodeList_Insert(list, (*it)->id, ++nr_nodes);
        if (unlikely(!list)) {
            return SELVA_MODIFY_HIERARCHY_ENOMEM;
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
 * BFS from a given head node towards its descendants or ancestors.
 */
static int bfs(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *head, enum SelvaModify_HierarchyNode_Relationship dir, const TraversalCallback * restrict cb) {
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
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
    }

    svector_autofree SVector q;
    if (unlikely(!SVector_Init(&q, HIERARCHY_EXPECTED_RESP_LEN, NULL))) {
        return SELVA_MODIFY_HIERARCHY_ENOMEM;
    }

    Trx_Begin(&hierarchy->current_trx);
    Trx_Stamp(&hierarchy->current_trx, &head->visit_stamp);
    SVector_Insert(&q, head);

    head_cb(head, cb->head_arg);

    while (SVector_Size(&q) > 0) {
        SelvaModify_HierarchyNode *node = SVector_Shift(&q);
        SelvaModify_HierarchyNode **itt;

        if (node_cb(node, cb->node_arg)) {
            return 0;
        }

        SVECTOR_FOREACH(itt, (const SVector *)((char *)node + offset)) {
            SelvaModify_HierarchyNode *it = *itt;

            if (!Trx_IsStamped(&hierarchy->current_trx, &it->visit_stamp)) {
                Trx_Stamp(&hierarchy->current_trx, &it->visit_stamp);

                child_cb(node, it, cb->child_arg);

                SVector_Insert(&q, it);
            }
        }
    }

    return 0;
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
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
    }

    svector_autofree SVector stack;
    if (unlikely(!SVector_Init(&stack, HIERARCHY_EXPECTED_RESP_LEN, NULL))) {
        return SELVA_MODIFY_HIERARCHY_ENOMEM;
    }

    Trx_Begin(&hierarchy->current_trx);
    SVector_Insert(&stack, head);

    head_cb(head, cb->head_arg);

    while (SVector_Size(&stack) > 0) {
        SelvaModify_HierarchyNode *node = SVector_Pop(&stack);

        if (!Trx_IsStamped(&hierarchy->current_trx, &node->visit_stamp)) {
            /* Mark node as visited */
            Trx_Stamp(&hierarchy->current_trx, &node->visit_stamp);

            if (node_cb(node, cb->node_arg)) {
                return 0;
            }

            /* Add parents/children of this node to the stack of unvisited nodes */
            SelvaModify_HierarchyNode **itt;
            const SVector *vec = (SVector *)((char *)node + offset);
            SVECTOR_FOREACH(itt, vec) {
                SelvaModify_HierarchyNode *it = *itt;

                child_cb(node, it, cb->child_arg);

                /* Add to the stack of unvisited nodes */
                SVector_Insert(&stack, it);
            }
        }
    }

    return 0;
}

/**
 * Traverse through all nodes of the hierarchy from heads to leaves.
 */
static int full_dfs(SelvaModify_Hierarchy *hierarchy, const TraversalCallback * restrict cb) {
    SelvaModify_HierarchyNode **head;
    svector_autofree SVector stack;

    HierarchyNode_HeadCallback head_cb = cb->head_cb ? cb->head_cb : HierarchyNode_HeadCallback_Dummy;
    HierarchyNode_Callback node_cb = cb->node_cb ? cb->node_cb : HierarchyNode_Callback_Dummy;
    HierarchyNode_ChildCallback child_cb = cb->child_cb ? cb->child_cb : HierarchyNode_ChildCallback_Dummy;

    if (unlikely(!SVector_Init(&stack, HIERARCHY_EXPECTED_RESP_LEN, NULL))) {
        return SELVA_MODIFY_HIERARCHY_ENOMEM;
    }

    Trx_Begin(&hierarchy->current_trx);

    SVECTOR_FOREACH(head, &hierarchy->heads) {
        SVector_Insert(&stack, *head);

        head_cb(*head, cb->head_arg);

        while (SVector_Size(&stack) > 0) {
            SelvaModify_HierarchyNode *node = SVector_Pop(&stack);

            if (!Trx_IsStamped(&hierarchy->current_trx, &node->visit_stamp)) {
                /* Mark node as visited */
                Trx_Stamp(&hierarchy->current_trx, &node->visit_stamp);

                if (node_cb(node, cb->node_arg)) {
                    return 0;
                }

                SelvaModify_HierarchyNode **itt;
                SVECTOR_FOREACH(itt, &node->children) {
                    SelvaModify_HierarchyNode *it = *itt;

                    child_cb(node, it, cb->child_arg);

                    /* Add to the stack of unvisited nodes */
                    SVector_Insert(&stack, it);
                }
            }
        }
    }

    return 0;
}

const char *SelvaModify_HierarchyDir2str(enum SelvaModify_HierarchyTraversal dir) {
    switch (dir) {
    case SELVA_HIERARCHY_TRAVERSAL_NONE:
        return "none";
    case SELVA_HIERARCHY_TRAVERSAL_NODE:
        return "node";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
        return "bfs_ancestors";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
        return "bfs_descendants";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
        return "dfs_ancestors";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
        return "dfs_descendants";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_FULL:
        return "dfs_full";
    default:
        return "invalid";
    }
}

static int traverse_adjacent(
        SelvaModify_HierarchyNode *head,
        enum SelvaModify_HierarchyTraversal dir,
        const TraversalCallback *tcb) {
    SVector *adjVec;
    SelvaModify_HierarchyNode **it;

    assert(tcb->node_cb);
    if (dir == SELVA_HIERARCHY_TRAVERSAL_CHILDREN) {
        adjVec = &head->children;
    } else if (dir == SELVA_HIERARCHY_TRAVERSAL_PARENTS) {
        adjVec = &head->parents;
    } else {
        return SELVA_MODIFY_HIERARCHY_EINVAL;
    }

    SVECTOR_FOREACH(it, adjVec) {
        SelvaModify_HierarchyNode *node = *it;

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
        const char *ref_field,
        const struct SelvaModify_HierarchyCallback *cb) {
    RedisModuleKey *key;

    key = SelvaSet_Open(ctx, head->id, Selva_NodeIdLen(head->id), ref_field);
    if (!key) {
        return SELVA_MODIFY_HIERARCHY_EINVAL;
    }

    RedisModule_ZsetFirstInScoreRange(key, 0, 1, 0, 0);
    while (!RedisModule_ZsetRangeEndReached(key)) {
        double score;
        RedisModuleString *ele;
        Selva_NodeId nodeId;
        SelvaModify_HierarchyNode *node;

        ele = RedisModule_ZsetRangeCurrentElement(key, &score);
        TO_STR(ele);
        memset(nodeId, 0, SELVA_NODE_ID_SIZE);
        memcpy(nodeId, ele_str, min(ele_len, SELVA_NODE_ID_SIZE));
        RedisModule_FreeString(ctx, ele);

        node = findNode(hierarchy, nodeId);
        if (node) {
            cb->node_cb(nodeId, cb->node_arg, &node->metadata);
        }

        RedisModule_ZsetRangeNext(key);
    }
    RedisModule_ZsetRangeStop(key);

    return 0;
}

/*
 * A little trampoline to hide the scary internals of the hierarchy
 * implementation from the innocent users just wanting to traverse the
 * hierarchy.
 */
static int SelvaModify_TraverseHierarchy_cb_wrapper(SelvaModify_HierarchyNode *node, void *arg) {
    struct SelvaModify_HierarchyCallback *cb = (struct SelvaModify_HierarchyCallback *)arg;

    return cb->node_cb(node->id, cb->node_arg, &node->metadata);
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
        return SELVA_MODIFY_HIERARCHY_EINVAL;
    }
    if (unlikely(dir == SELVA_HIERARCHY_TRAVERSAL_REF)) {
        /* Use SelvaModify_TraverseHierarchyRef() instead */
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
    }

    if (dir != SELVA_HIERARCHY_TRAVERSAL_DFS_FULL) {
        head = findNode(hierarchy, id);
        if (!head) {
            return SELVA_MODIFY_HIERARCHY_ENOENT;
        }
    }

    switch (dir) {
    case SELVA_HIERARCHY_TRAVERSAL_NODE:
        cb->node_cb(head->id, cb->node_arg, &head->metadata);
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
        fprintf(stderr, "%s: Invalid traversal requested\n", __FILE__);
        err = SELVA_MODIFY_HIERARCHY_ENOTSUP;
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

    head = findNode(hierarchy, id);
    if (!head) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }

    return traverse_ref(ctx, hierarchy, head, ref_field, cb);
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
            *nr_nodes = SELVA_MODIFY_HIERARCHY_ENOMEM;
        }
    }

    return 0;
}

static ssize_t SelvaModify_FindDir(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, enum SelvaModify_HierarchyNode_Relationship dir, Selva_NodeId **res) {
    SelvaModify_HierarchyNode *head = findNode(hierarchy, id);
    if (!head) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
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

void *HierarchyTypeRDBLoad(RedisModuleIO *io, int encver) {
    if (encver != HIERARCHY_ENCODING_VERSION) {
        /*
         * We should actually log an error here, or try to implement
         * the ability to load older versions of our data structure.
         */
        return NULL;
    }
    rdbLoading = 1;
    SelvaModify_Hierarchy *hierarchy = SelvaModify_NewHierarchy(NULL);

    while (1) {
        int err;
        size_t len;
        char *node_id __attribute__((cleanup(wrapFree))) = RedisModule_LoadStringBuffer(io, &len);

        if (len != SELVA_NODE_ID_SIZE) {
            goto error;
        }

        if (!memcmp(node_id, HIERARCHY_RDB_EOF, SELVA_NODE_ID_SIZE)) {
            break;
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
                char *child_id __attribute__((cleanup(wrapFree))) = RedisModule_LoadStringBuffer(io, &len);

                if (len != SELVA_NODE_ID_SIZE) {
                    goto error;
                }

                err = SelvaModify_AddHierarchy(NULL, hierarchy, child_id, 0, NULL, 0, NULL);
                if (err) {
                    RedisModule_LogIOError(io, "warning", "Unable to rebuild the hierarchy");
                    return NULL;
                }

                memcpy(children + i, child_id, SELVA_NODE_ID_SIZE);
            }
        }

        /* Create the node itself */
        err = SelvaModify_AddHierarchy(NULL, hierarchy, node_id, 0, NULL, nr_children, children);
        if (err) {
            RedisModule_LogIOError(io, "warning", "Unable to rebuild the hierarchy");
            return NULL;
        }
    }

    rdbLoading = 0;

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Update depths on a single pass to save time.
     */
    SelvaModify_HierarchyNode **it;
    SVECTOR_FOREACH(it, &hierarchy->heads) {
        updateDepth(hierarchy, *it);
    }
#endif

    return hierarchy;
error:
    SelvaModify_DestroyHierarchy(hierarchy);

    return NULL;
}

static int HierarchyRDBSaveNode(SelvaModify_HierarchyNode *node, void *arg) {
    RedisModuleIO *io = (RedisModuleIO *)arg;

    RedisModule_SaveStringBuffer(io, node->id, SELVA_NODE_ID_SIZE);
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
     * NODE_ID1 | NR_CHILDREN | CHILD_ID_0,..
     * NODE_ID2 | NR_CHILDREN | ...
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

/* FIXME Add gets stuck if parents are missing */
int SelvaModify_Hierarchy_AddNodeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    SelvaModify_Hierarchy *hierarchy;

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if (!hierarchy) {
        return replyWithSelvaError(ctx, SELVA_MODIFY_HIERARCHY_ENOENT);
    }

    /*
     * Insert the new element.
     */
    Selva_NodeId nodeId;
    Selva_NodeId *parents = NULL;
    const size_t nr_parents = argc - 3;

    RMString2NodeId(nodeId, argv[2]);
    parents = RedisModule_PoolAlloc(ctx, nr_parents * sizeof(Selva_NodeId));
    if (!parents) {
        return REDISMODULE_ERR;
    }

    for (size_t i = 0; i < nr_parents; i++) {
        RMString2NodeId(parents[i], argv[3 + i]);
    }

    int err = SelvaModify_AddHierarchy(ctx, hierarchy, nodeId, nr_parents, parents, 0, NULL);
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    RedisModule_ReplyWithLongLong(ctx, 1);
    RedisModule_ReplicateVerbatim(ctx);
    SelvaSubscriptions_SendDeferredEvents(hierarchy);

    return REDISMODULE_OK;
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
    SelvaModify_Hierarchy *hierarchy = SelvaModify_OpenHierarchy(ctx, argv[1], REDISMODULE_READ);
    if (!hierarchy) {
        return REDISMODULE_OK;
    }

    long long nr_deleted = 0;
    for (size_t i = 2; i < (size_t)argc; i++) {
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

    SelvaModify_HierarchyNode *node = findNode(hierarchy, nodeId);
    if (!node) {
        RedisModule_ReplyWithLongLong(ctx, 0);
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
            return replyWithSelvaError(ctx, SELVA_MODIFY_HIERARCHY_ENOMEM);
        }

        for (size_t i = 0; i < ids_len; i++) {
            Selva_NodeId nodeId;

            memcpy(nodeId, ids + i, SELVA_NODE_ID_SIZE);

            /*
             * Find the node.
             */
            SelvaModify_HierarchyNode *child = findNode(hierarchy, nodeId);
            if (!child) {
                /* Node not found;
                 * This is probably fine, as there might have been a circular link.
                 */
                continue;
            }

            if (SVector_Size(&child->parents) == 0) {
                /* TODO Ignoring errors for now. */
                (void)SelvaModify_DelHierarchyNodeP(ctx, hierarchy, child);
            }
        }
    } else {
        return replyWithSelvaError(ctx, SELVA_MODIFY_HIERARCHY_ENOTSUP);
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
    SelvaModify_HierarchyNode *node = findNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_MODIFY_HIERARCHY_ENOENT);
    }

#if HIERARCHY_SORT_BY_DEPTH
    svector_autofree SVector parents;

    if (unlikely(!SVector_Clone(&parents, &node->parents, SVector_HierarchyNode_depth_compare))) {
        return replyWithSelvaError(ctx, SELVA_MODIFY_HIERARCHY_ENOMEM);
    }

    RedisModule_ReplyWithArray(ctx, SVector_Size(&parents));

    SelvaModify_HierarchyNode **itt;
    SVECTOR_FOREACH(itt, &parents) {
        SelvaModify_HierarchyNode *it = *itt;

        RedisModule_ReplyWithStringBuffer(ctx, it->id, Selva_NodeIdLen(it->id));
    }
#else
    RedisModule_ReplyWithArray(ctx, SVector_Size(&node->parents));

    SelvaModify_HierarchyNode **itt;
    SVECTOR_FOREACH(itt, &node->parents) {
        SelvaModify_HierarchyNode *it = *itt;

        RedisModule_ReplyWithStringBuffer(ctx, it->id, Selva_NodeIdLen(it->id));
    }
#endif

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
    SelvaModify_HierarchyNode *node = findNode(hierarchy, nodeId);
    if (!node) {
        return replyWithSelvaError(ctx, SELVA_MODIFY_HIERARCHY_ENOENT);
    }

    RedisModule_ReplyWithArray(ctx, SVector_Size(&node->children));

    SelvaModify_HierarchyNode **itt;
    SVECTOR_FOREACH(itt, &node->children) {
        SelvaModify_HierarchyNode *it = *itt;

        RedisModule_ReplyWithStringBuffer(ctx, it->id, Selva_NodeIdLen(it->id));
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
    if (RedisModule_CreateCommand(ctx, "selva.hierarchy.add", SelvaModify_Hierarchy_AddNodeCommand,     "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.del", SelvaModify_Hierarchy_DelNodeCommand,     "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.delref", SelvaModify_Hierarchy_DelRefCommand,   "write deny-oom", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.parents", SelvaModify_Hierarchy_ParentsCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.children", SelvaModify_Hierarchy_ChildrenCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Hierarchy_OnLoad);
