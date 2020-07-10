#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "redismodule.h"
#include "cdefs.h"
#include "tree.h"
#include "svector.h"
#include "trx.h"
#include "rpn.h"
#include "hierarchy.h"

#define HIERARCHY_ENCODING_VERSION  0

#pragma GCC diagnostic ignored "-Wstringop-truncation"

typedef struct SelvaModify_HierarchyNode {
    Selva_NodeId id;
    struct timespec visit_stamp;
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

RB_HEAD(hierarchy_index_tree, SelvaModify_HierarchyNode);

typedef struct SelvaModify_Hierarchy {
    /**
     * Current transaction timestamp.
     * Set before traversal begins and is used for marking visited nodes. Due to the
     * marking being a timestamp it's not necessary to clear it afterwards, which
     * could be a costly operation itself.
     */
    Trx current_trx;
    struct hierarchy_index_tree index_head;
    /**
     * Orphan nodes aka heads of the hierarchy.
     */
    SVector heads;
} SelvaModify_Hierarchy;

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

static const Selva_NodeId HIERARCHY_RDB_EOF __attribute__((nonstring));
static RedisModuleType *HierarchyType;

const char * const hierarchyStrError[] = {
    (const char *)"HIERARCHY_ERR No Error",
    (const char *)"HIERARCHY_ERR EGENERAL Unknown error",
    (const char *)"HIERARCHY_ERR ENOTSUP Operation not supported",
    (const char *)"HIERARCHY_ERR ENOMEM Out of memory",
    (const char *)"HIERARCHY_ERR ENOENT Not found",
    (const char *)"HIERARCHY_ERR EEXIST Exist",
};

static void SelvaModify_DestroyNode(SelvaModify_HierarchyNode *node);
static void removeRelationships(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node, enum SelvaModify_HierarchyNode_Relationship rel);
RB_PROTOTYPE_STATIC(hierarchy_index_tree, SelvaModify_HierarchyNode, _index_entry, SelvaModify_HierarchyNode_Compare)

static int SVector_BS_Compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const SelvaModify_HierarchyNode *a = *(const SelvaModify_HierarchyNode **)a_raw;
    const SelvaModify_HierarchyNode *b = *(const SelvaModify_HierarchyNode **)b_raw;

    return memcmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

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

static void RMString2NodeId(Selva_NodeId nodeId, RedisModuleString *rmStr) {
        const char *str;
        size_t slen;

        str = RedisModule_StringPtrLen(rmStr, &slen);

        memset(nodeId, '\0', SELVA_NODE_ID_SIZE);
        memcpy(nodeId, str, min(slen, SELVA_NODE_ID_SIZE));
}

/*
 * Technically a nodeId is always 10 bytes but sometimes a printable
 * representation without padding zeroes is needed.
 */
static size_t SelvaModify_NodeIdLen(Selva_NodeId nodeId) {
    size_t len = SELVA_NODE_ID_SIZE;

    while (len >= 1 && nodeId[len - 1] == '\0') {
        len--;
    }

    return len;
}

SelvaModify_Hierarchy *SelvaModify_NewHierarchy(void) {
    SelvaModify_Hierarchy *hierarchy = RedisModule_Alloc(sizeof(SelvaModify_HierarchyNode));
    if (unlikely(!hierarchy)) {
        return NULL;
    }

    RB_INIT(&hierarchy->index_head);
    if (unlikely(!SVector_Init(&hierarchy->heads, 1, SVector_BS_Compare))) {
        RedisModule_Free(hierarchy);
        return NULL;
    }

    if(unlikely(SelvaModify_SetHierarchy(hierarchy, ROOT_NODE_ID, 0, NULL, 0, NULL))) {
        SelvaModify_DestroyHierarchy(hierarchy);
        return NULL;
    }

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

    SVector_Destroy(&hierarchy->heads);
    RedisModule_Free(hierarchy);
}

SelvaModify_Hierarchy *SelvaModify_OpenHierarchyKey(RedisModuleCtx *ctx, RedisModuleString *key_name) {
    SelvaModify_Hierarchy *hierarchy;
    RedisModuleKey *key;
    int type;

    key = RedisModule_OpenKey(ctx, key_name, REDISMODULE_READ | REDISMODULE_WRITE);
    type = RedisModule_KeyType(key);

    if (type != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != HierarchyType) {
        RedisModule_CloseKey(key);
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);

        return NULL;
    }

    /* Create an empty value object if the key is currently empty. */
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        hierarchy = SelvaModify_NewHierarchy();
        if (!hierarchy) {
            RedisModule_ReplyWithError(ctx, hierarchyStrError[-SELVA_MODIFY_HIERARCHY_ENOMEM]);

            return NULL;
        }

        RedisModule_ModuleTypeSetValue(key, HierarchyType, hierarchy);
    } else {
        hierarchy = RedisModule_ModuleTypeGetValue(key);
    }

    return hierarchy;
}

static SelvaModify_HierarchyNode *newNode(const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node = RedisModule_Alloc(sizeof(SelvaModify_HierarchyNode));
    if (unlikely(!node)) {
        return NULL;
    };

    memset(node, 0, sizeof(SelvaModify_HierarchyNode));

    if (unlikely(!SVector_Init(&node->parents, HIERARCHY_INITIAL_VECTOR_LEN, SVector_BS_Compare) ||
        !SVector_Init(&node->children, HIERARCHY_INITIAL_VECTOR_LEN, SVector_BS_Compare))) {
        SelvaModify_DestroyNode(node);
        return NULL;
    }

    memcpy(node->id, id, SELVA_NODE_ID_SIZE);

    return node;
}

static void SelvaModify_DestroyNode(SelvaModify_HierarchyNode *node) {
    SVector_Destroy(&node->parents);
    SVector_Destroy(&node->children);
    RedisModule_Free(node);
}

static void del_node(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node) {
    removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);
    removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);
    RB_REMOVE(hierarchy_index_tree, &hierarchy->index_head, node);
    SelvaModify_DestroyNode(node);
}

static SelvaModify_HierarchyNode *findNode(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
        SelvaModify_HierarchySearchFilter filter;

        memcpy(&filter.id, id, SELVA_NODE_ID_SIZE);
        return RB_FIND(hierarchy_index_tree, &hierarchy->index_head, (SelvaModify_HierarchyNode *)(&filter));
}

static void mkHead(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node) {
    SVector_Insert(&hierarchy->heads, node);
}

static void rmHead(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node) {
    SVector_Remove(&hierarchy->heads, node);
}

static int crossInsert(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node, enum SelvaModify_HierarchyNode_Relationship rel, size_t n, const Selva_NodeId *nodes) {
    const size_t initialNodeParentsSize = SVector_Size(&node->parents);
    int err = 0;

    if (rel == RELATIONSHIP_CHILD && n > 0 && initialNodeParentsSize == 0) {
        /* The node is no longer an orphan */
        rmHead(hierarchy, node);
    }

    if (rel == RELATIONSHIP_CHILD) { /* node is a child to adjacent */
        for (size_t i = 0; i < n; i++) {
            SelvaModify_HierarchyNode *adjacent = findNode(hierarchy, nodes[i]);

            if (!adjacent) {
                err = SELVA_MODIFY_HIERARCHY_ENOENT;
                continue;
            }

            /* Do inserts only if the relationship doesn't exist already */
            if (initialNodeParentsSize == 0 || !SVector_Search(&node->parents, adjacent)) {
                SVector_Insert(&node->parents, adjacent);
                SVector_Insert(&adjacent->children, node);
            }
        }
    } else if (rel == RELATIONSHIP_PARENT) { /* node is a parent to adjacent */
        for (size_t i = 0; i < n; i++) {
            SelvaModify_HierarchyNode *adjacent = findNode(hierarchy, nodes[i]);

            if (!adjacent) {
                err = SELVA_MODIFY_HIERARCHY_ENOENT;
                continue;
            }

            const size_t adjNodeParentsSize = SVector_Size(&adjacent->parents);

            /* The adjacent node is no longer an orphan */
            if (adjNodeParentsSize == 0) {
                rmHead(hierarchy, adjacent);
            }

            if (adjNodeParentsSize == 0 || !SVector_Search(&adjacent->parents, node)) {
                SVector_Insert(&node->children, adjacent);
                SVector_Insert(&adjacent->parents, node);
            }
        }
    } else {
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
    }

    return err;
}

static int crossRemove(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node, enum SelvaModify_HierarchyNode_Relationship rel, size_t n, const Selva_NodeId *nodes) {
    if (rel == RELATIONSHIP_CHILD) { /* no longer a child of adjacent */
        const size_t initialNodeParentsSize = SVector_Size(&node->parents);

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
        }

        if (initialNodeParentsSize > 0 && SVector_Size(&node->parents) == 0) {
            /* node is an orphan now */
            mkHead(hierarchy, node);
        }
    } else if (rel == RELATIONSHIP_PARENT) { /* no longer a parent of adjacent */
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
        }
    } else {
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
    }

    return 0;
}

/**
 * Remove all relationships rel of node.
 */
static void removeRelationships(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node, enum SelvaModify_HierarchyNode_Relationship rel) {
    size_t offset_a;
    size_t offset_b;

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
    }
    SVector_Clear(vec_a);

    /*
     * After this the caller should call mkHead(hierarchy, node)
     * if rel == RELATIONSHIP_CHILD.
     */
}

int SelvaModify_SetHierarchy(
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
        node = newNode(id);
        if (unlikely(!node)) {
            return SELVA_MODIFY_HIERARCHY_ENOENT;
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
     * TODO if isNewNode == 0 then errors are not handled properly as
     * we don't know how to rollback.
     */
    err = crossInsert(hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents);
    if (err) {
        if (isNewNode) {
            del_node(hierarchy, node);
        }
        return err;
    }
    err = crossInsert(hierarchy, node, RELATIONSHIP_PARENT, nr_children, children);
    if (err) {
        if (isNewNode) {
            del_node(hierarchy, node);
        }
        return err;
    }

    return 0;
}

int SelvaModify_SetHierarchyParents(
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
    removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);

    if (nr_parents == 0) {
        /* This node is orphan */
        mkHead(hierarchy, node);
    }

    /*
     * Set relationship relative to other nodes
     */
    err = crossInsert(hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents);
    if (err) {
        return err;
    }

    return 0;
}

int SelvaModify_SetHierarchyChildren(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_children,
        const Selva_NodeId *children) {
    int err;
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);

    if (!node) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }

    removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);

    /*
     * Set relationship relative to other nodes
     */
    err = crossInsert(hierarchy, node, RELATIONSHIP_PARENT, nr_children, children);
    if (err) {
        return err;
    }

    return 0;
}

int SelvaModify_AddHierarchy(
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
        node = newNode(id);
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
     * TODO if isNewNode == 0 then errors are not handled properly as
     * we don't know how to rollback.
     */
    err = crossInsert(hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents);
    if (err && isNewNode) {
        del_node(hierarchy, node);
        return err;
    }
    err = crossInsert(hierarchy, node, RELATIONSHIP_PARENT, nr_children, children);
    if (err && isNewNode) {
        del_node(hierarchy, node);
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

    if (!node) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }

    /*
     * The most Redis thing to do is probably to ignore any
     * missing nodes.
     */
    (void)crossRemove(hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents);
    (void)crossRemove(hierarchy, node, RELATIONSHIP_PARENT, nr_children, children);

    return 0;
}

int SelvaModify_DelHierarchyNode(
    SelvaModify_Hierarchy *hierarchy,
    const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);

    if (!node) {
        return SELVA_MODIFY_HIERARCHY_ENOENT;
    }

    del_node(hierarchy, node);

    return 0;
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

    Trx_Begin(&hierarchy->current_trx);

    SVector stack;
    SVector_Init(&stack, HIERARCHY_EXPECTED_RESP_LEN, NULL);
    SVector_Insert(&stack, head);

    head_cb(head, cb->head_arg);

    while (SVector_Size(&stack) > 0) {
        SelvaModify_HierarchyNode *node = SVector_Pop(&stack);

        if (!Trx_IsStamped(&hierarchy->current_trx, &node->visit_stamp)) {
            /* Mark node as visited */
            Trx_Stamp(&hierarchy->current_trx, &node->visit_stamp);

            if (node_cb(node, cb->node_arg)) {
                goto out;
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

out:
    SVector_Destroy(&stack);
    return 0;
}

/**
 * Traverse through all nodes of the hierarchy from heads to leaves.
 */
static void full_dfs(SelvaModify_Hierarchy *hierarchy, const TraversalCallback * restrict cb) {
    SelvaModify_HierarchyNode **head;
    SVector stack;

    HierarchyNode_HeadCallback head_cb = cb->head_cb ? cb->head_cb : HierarchyNode_HeadCallback_Dummy;
    HierarchyNode_Callback node_cb = cb->node_cb ? cb->node_cb : HierarchyNode_Callback_Dummy;
    HierarchyNode_ChildCallback child_cb = cb->child_cb ? cb->child_cb : HierarchyNode_ChildCallback_Dummy;

    SVector_Init(&stack, HIERARCHY_EXPECTED_RESP_LEN, NULL);
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
                    goto out;
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

out:
    SVector_Destroy(&stack);
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

    int err = dfs(hierarchy, head, dir, &cb);

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
    SelvaModify_Hierarchy *hierarchy = SelvaModify_NewHierarchy();

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

                err = SelvaModify_AddHierarchy(hierarchy, child_id, 0, NULL, 0, NULL);
                if (err) {
                    RedisModule_LogIOError(io, "warning", "Unable to rebuild the hierarchy");
                    return NULL;
                }

                memcpy(children + i, child_id, SELVA_NODE_ID_SIZE);
            }
        }

        /* Create the node itself */
        err = SelvaModify_AddHierarchy(hierarchy, node_id, 0, NULL, nr_children, children);
        if (err) {
            RedisModule_LogIOError(io, "warning", "Unable to rebuild the hierarchy");
            return NULL;
        }
    }

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
    full_dfs(hierarchy, &cb);

    RedisModule_SaveStringBuffer(io, HIERARCHY_RDB_EOF, sizeof(HIERARCHY_RDB_EOF));
}

static void HierarchyAOFSaveHead(SelvaModify_HierarchyNode *node, void *arg) {
    void **args = (void **)arg;
    RedisModuleIO *aof = (RedisModuleIO *)args[0];
    RedisModuleString *key = (RedisModuleString *)args[1];

    /* Create the head node */
    RedisModule_EmitAOF(aof,"SELVA.HIERARCHY.ADD", "ss", key, node->id);
}

static void HierarchyAOFSaveChild(SelvaModify_HierarchyNode *parent, SelvaModify_HierarchyNode *child, void *arg) {
    void **args = (void **)arg;
    RedisModuleIO *aof = (RedisModuleIO *)args[0];
    RedisModuleString *key = (RedisModuleString *)args[1];

    /* Create the children */
    RedisModule_EmitAOF(aof,"SELVA.HIERARCHY.ADD", "ss", key, child->id, parent->id);
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

    full_dfs(hierarchy, &cb);
}

void HierarchyTypeFree(void *value) {
    SelvaModify_Hierarchy *hierarchy = (SelvaModify_Hierarchy *)value;

    SelvaModify_DestroyHierarchy(hierarchy);
}

int SelvaModify_Hierarchy_AddNodeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    SelvaModify_Hierarchy *hierarchy;

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    hierarchy = SelvaModify_OpenHierarchyKey(ctx, argv[1]);
    if (!hierarchy) {
        return REDISMODULE_ERR;
    }

    /*
     * Insert the new element.
     */
    Selva_NodeId nodeId;
    Selva_NodeId *parents __attribute__((cleanup(wrapFree))) = NULL;
    const size_t nr_parents = argc - 3;

    RMString2NodeId(nodeId, argv[2]);
    parents = RedisModule_Calloc(nr_parents, sizeof(Selva_NodeId));
    if (!parents) {
        return REDISMODULE_ERR;
    }

    for (size_t i = 0; i < nr_parents; i++) {
        RMString2NodeId(parents[i], argv[3 + i]);
    }

    int err = SelvaModify_AddHierarchy(hierarchy, nodeId, nr_parents, parents, 0, NULL);
    if (err) {
        return RedisModule_ReplyWithError(ctx, hierarchyStrError[-err]);
    }

    RedisModule_ReplyWithLongLong(ctx, 1);
    RedisModule_ReplicateVerbatim(ctx);
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
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != HierarchyType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    /* Create an empty value object if the key is currently empty. */
    SelvaModify_Hierarchy *hierarchy = RedisModule_ModuleTypeGetValue(key);
    if (!hierarchy) {
        return RedisModule_ReplyWithArray(ctx, 0);
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    RMString2NodeId(nodeId, argv[2]);
    SelvaModify_HierarchyNode *node = findNode(hierarchy, nodeId);
    if (!node) {
        return RedisModule_ReplyWithError(ctx, hierarchyStrError[-SELVA_MODIFY_HIERARCHY_ENOENT]);
    }

    RedisModule_ReplyWithArray(ctx, SVector_Size(&node->parents));

    SelvaModify_HierarchyNode **itt;
    SVECTOR_FOREACH(itt, &node->parents) {
        SelvaModify_HierarchyNode *it = *itt;

        RedisModule_ReplyWithStringBuffer(ctx, it->id, SelvaModify_NodeIdLen(it->id));
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
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != HierarchyType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    /* Create an empty value object if the key is currently empty. */
    SelvaModify_Hierarchy *hierarchy = RedisModule_ModuleTypeGetValue(key);
    if (!hierarchy) {
        return RedisModule_ReplyWithArray(ctx, 0);
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    RMString2NodeId(nodeId, argv[2]);
    SelvaModify_HierarchyNode *node = findNode(hierarchy, nodeId);
    if (!node) {
        return RedisModule_ReplyWithError(ctx, hierarchyStrError[-SELVA_MODIFY_HIERARCHY_ENOENT]);
    }

    RedisModule_ReplyWithArray(ctx, SVector_Size(&node->children));

    SelvaModify_HierarchyNode **itt;
    SVECTOR_FOREACH(itt, &node->children) {
        SelvaModify_HierarchyNode *it = *itt;

        RedisModule_ReplyWithStringBuffer(ctx, it->id, SelvaModify_NodeIdLen(it->id));
    }

    return REDISMODULE_OK;
}

static int parse_dfs_dir(enum SelvaModify_HierarchyNode_Relationship *dir, RedisModuleString *arg) {
    size_t len;
    const char *str = RedisModule_StringPtrLen(arg, &len);

    if (!strncmp("ancestors", str, len)) {
        *dir = RELATIONSHIP_PARENT;
    } else if (!strncmp("descendants", str, len)) {
        *dir = RELATIONSHIP_CHILD;
    } else {
        return SELVA_MODIFY_HIERARCHY_ENOTSUP;
    }

    return 0;
}

struct FindCommand_Args {
    RedisModuleCtx *ctx;
    SelvaModify_HierarchyNode *head;
    ssize_t *nr_nodes;
    struct rpn_ctx *rpn_ctx;
    const rpn_token *filter;
};

static int FindCommand_PrintNode(SelvaModify_HierarchyNode *node, void *arg) {
    struct FindCommand_Args *args = (struct FindCommand_Args *)arg;
    struct rpn_ctx *rpn_ctx = args->rpn_ctx;

    if (likely(node != args->head)) {
        int take = 1;

        if (rpn_ctx) {
            int err;

            /* Set node_id to the register */
            rpn_set_reg(rpn_ctx, 0, node->id, SELVA_NODE_ID_SIZE);

            /*
             * Resolve the expression and get the result.
             */
            err = rpn_bool(rpn_ctx, args->filter, &take);
            if (err) {
                fprintf(stderr, "Expression failed: \"%s\"\n",
                        rpn_str_error[err]);
                /*
                 * TODO Propagate error?
                 * It would be a good idea to propagate the error but Redis
                 * doesn't actually support sending an error in the middle
                 * of an array response.
                 */
                return 1;
            }
        }

        if (take) {
            ssize_t *nr_nodes = args->nr_nodes;

            RedisModule_ReplyWithStringBuffer(args->ctx, node->id, SelvaModify_NodeIdLen(node->id));
            *nr_nodes = *nr_nodes + 1;
        }
    }

    return 0;
}

/**
 * Find node ancestors/descendants.
 * SELVA.HIERARCHY.find REDIS_KEY DESCENDANTS|ANCESTORS NODE_ID [filter expression] [args...]
 */
int SelvaModify_Hierarchy_FindCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int err;

    const size_t ARGV_REDIS_KEY    = 1;
    const size_t ARGV_DIRECTION    = 2;
    const size_t ARGV_NODE_ID      = 3;
    const size_t ARGV_FILTER_EXPR  = 4;
    const size_t ARGV_FILTER_ARGS  = 5;

    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != HierarchyType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    /* Create an empty value object if the key is currently empty. */
    SelvaModify_Hierarchy *hierarchy = RedisModule_ModuleTypeGetValue(key);
    if (!hierarchy) {
        return RedisModule_ReplyWithArray(ctx, 0);
    }

    /*
     * Get the direction parameter.
     */
    enum SelvaModify_HierarchyNode_Relationship dir;
    err = parse_dfs_dir(&dir, argv[ARGV_DIRECTION]);
    if (err) {
        return RedisModule_ReplyWithError(ctx, hierarchyStrError[-err]);
    }

    /*
     * Find the node.
     */
    Selva_NodeId nodeId;

    RMString2NodeId(nodeId, argv[ARGV_NODE_ID]);
    SelvaModify_HierarchyNode *head = findNode(hierarchy, nodeId);
    if (!head) {
        return RedisModule_ReplyWithError(ctx, hierarchyStrError[-SELVA_MODIFY_HIERARCHY_ENOENT]);
    }

    /*
     * Prepare the filter expression if given.
     */
    struct rpn_ctx *rpn_ctx = NULL;
    rpn_token *filter_expression = NULL;
    if (argc >= (int)ARGV_FILTER_EXPR + 1) {
        const int nr_reg = argc - ARGV_FILTER_ARGS + 2;
        const char *input;
        size_t input_len;

        rpn_ctx = rpn_init(ctx, nr_reg);
        if (!rpn_ctx) {
            return RedisModule_ReplyWithError(ctx, hierarchyStrError[-SELVA_MODIFY_HIERARCHY_ENOMEM]);
        }

        /*
         * Get the filter expression arguments and set them to the registers.
         */
        for (size_t i = ARGV_FILTER_ARGS; i < (size_t)argc; i++) {
            /* reg[0] is reserved for the current nodeId */
            const size_t reg_i = i - ARGV_FILTER_ARGS + 1;
            size_t str_len;
            const char *str = RedisModule_StringPtrLen(argv[i], &str_len);

            rpn_set_reg(rpn_ctx, reg_i, str, str_len + 1);
        }

        input = RedisModule_StringPtrLen(argv[ARGV_FILTER_EXPR], &input_len);
        filter_expression = rpn_compile(input, input_len);
    }

    /*
     * Run DFS.
     */
    ssize_t nr_nodes = 0;
    struct FindCommand_Args args = {
        .ctx = ctx,
        .head = head,
        .nr_nodes = &nr_nodes,
        .rpn_ctx = rpn_ctx,
        .filter = filter_expression,
    };
    const TraversalCallback cb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = FindCommand_PrintNode,
        .node_arg = &args,
        .child_cb = NULL,
        .child_arg = NULL,
    };

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    err = dfs(hierarchy, head, dir, &cb);
    if (rpn_ctx) {
        RedisModule_Free(filter_expression);
        rpn_destroy(rpn_ctx);
    }
    if (err != 0) {
        /* FIXME This will make redis crash */
        return RedisModule_ReplyWithError(ctx, hierarchyStrError[-err]);
    }

    RedisModule_ReplySetArrayLength(ctx, nr_nodes);

    return REDISMODULE_OK;
}

static int DumpCommand_PrintNode(SelvaModify_HierarchyNode *node, void *arg) {
    void **args = (void **)arg;
    RedisModuleCtx *ctx = (RedisModuleCtx *)args[0];
    ssize_t * restrict nr_nodes = (ssize_t *)args[1];
    ssize_t * restrict cur_nr_children = (ssize_t *)args[2];
    ssize_t *stop = (ssize_t *)args[3];

    if (*nr_nodes == 0) {
        /*
         * This is is here because redis will crash if we'll call
         * RedisModule_ReplyWithError() after starting an array reply.
         * Hopefully the traversal function won't return an error after this.
         */
        RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    } else if (*nr_nodes > 0) {
        /* The parent node is the first node in the array, therefore + 1 */
        RedisModule_ReplySetArrayLength(ctx, *cur_nr_children + 1);
        *cur_nr_children = 0;
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    if (*stop > 0 && *nr_nodes >= *stop) {
        return 1;
    }

    RedisModule_ReplyWithStringBuffer(ctx, node->id, SelvaModify_NodeIdLen(node->id));
    *nr_nodes = *nr_nodes + 1;

    return 0;
}

static void DumpCommand_PrintChild(SelvaModify_HierarchyNode *parent, SelvaModify_HierarchyNode *child, void *arg) {
    REDISMODULE_NOT_USED(parent);
    void **args = (void **)arg;
    RedisModuleCtx *ctx = (RedisModuleCtx *)args[0];
    ssize_t *cur_nr_children = (ssize_t *)args[2];

    RedisModule_ReplyWithStringBuffer(ctx, child->id, SelvaModify_NodeIdLen(child->id));
    *cur_nr_children = *cur_nr_children + 1;
}

/*
 * KEY
 * KEY STOP_AFTER_N
 * KEY ANCESTORS|DESCENDANTS NODE_ID
 * KEY ANCESTORS|DESCENDANTS NODE_ID STOP_AFTER_N
 */
int SelvaModify_Hierarchy_DumpCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    RedisModuleString *op = NULL;
    RedisModuleString *keyName;
    ssize_t stop = -1;
    Selva_NodeId nodeId;
    enum SelvaModify_HierarchyNode_Relationship dir;

    if (argc == 3 || argc == 5) {
        const long long si = --argc;
        long long v;

        if (RedisModule_StringToLongLong(argv[si], &v) != REDISMODULE_OK) {
            RedisModule_ReplyWithError(ctx, "Invalid value for STOP");
        }

        if (v > 0) {
            stop = v;
        }
    }

    if (argc == 2) {
        keyName = argv[1];
    } else if (argc == 4) {
        int err;
        size_t len;
        const char *str;

        op = argv[2];
        keyName = argv[1];

        err = parse_dfs_dir(&dir, op);
        if (err) {
            return RedisModule_ReplyWithError(ctx, hierarchyStrError[-err]);
        }

        str = RedisModule_StringPtrLen(argv[3], &len);
        memset(nodeId, 0, SELVA_NODE_ID_SIZE);
        memcpy(nodeId, str, (SELVA_NODE_ID_SIZE > len) ? len : SELVA_NODE_ID_SIZE);
    } else {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);
    int type = RedisModule_KeyType(key);
    if (type != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != HierarchyType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    /* Create an empty value object if the key is currently empty. */
    SelvaModify_Hierarchy *hierarchy = RedisModule_ModuleTypeGetValue(key);
    if (!hierarchy) {
        return RedisModule_ReplyWithArray(ctx, 0);
    }

    ssize_t nr_nodes = 0;
    ssize_t cur_nr_children = 0;
    void *args[] = { ctx, &nr_nodes, &cur_nr_children, &stop };
    const TraversalCallback cb = {
        .head_cb = NULL,
        .head_arg = NULL,
        .node_cb = DumpCommand_PrintNode,
        .node_arg = args,
        .child_cb = DumpCommand_PrintChild,
        .child_arg = args,
    };

    if (op) {
        int err;

        SelvaModify_HierarchyNode *node = findNode(hierarchy, nodeId);
        if (!node) {
            return RedisModule_ReplyWithError(ctx, hierarchyStrError[-SELVA_MODIFY_HIERARCHY_ENOENT]);
        }

        err = dfs(hierarchy, node, dir, &cb);
        if (err < 0) {
            return RedisModule_ReplyWithError(ctx, hierarchyStrError[-err]);
        }
    } else {
        full_dfs(hierarchy, &cb);
    }

    if (nr_nodes > 0) {
        RedisModule_ReplySetArrayLength(ctx, cur_nr_children + 1);
    }

    RedisModule_ReplySetArrayLength(ctx, nr_nodes);

    return REDISMODULE_OK;
}

int Hierarchy_OnLoad(RedisModuleCtx *ctx) {
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
        RedisModule_CreateCommand(ctx, "selva.hierarchy.parents", SelvaModify_Hierarchy_ParentsCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.children", SelvaModify_Hierarchy_ChildrenCommand, "readonly fast", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.find", SelvaModify_Hierarchy_FindCommand,       "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.hierarchy.dump", SelvaModify_Hierarchy_DumpCommand,       "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
