#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "redismodule.h"
#include "cdefs.h"
#include "tree.h"
#include "svector.h"
#include "hierarchy.h"

#define INITIAL_VECTOR_LEN 2

typedef struct SelvaModify_HierarchyNode {
    Selva_NodeId id;
    struct timespec visit_stamp;
    Vector parents;
    Vector children;
    RB_ENTRY(SelvaModify_HierarchyNode) _index_entry;
} SelvaModify_HierarchyNode;

enum SelvaModify_HierarchyNode_Relationship {
    RELATIONSHIP_PARENT,
    RELATIONSHIP_CHILD,
};

typedef struct SelvaModify_HierarchySearchFilter {
    Selva_NodeId id;
} SelvaModify_HierarchySearchFilter;

/**
 * Current transaction timestamp.
 * Set before traversal begins and is used for marking visited nodes. Due to the
 * marking being a timestamp it's not necessary to clear it afterwards, which
 * could be a costly operation itself.
 */
static struct timespec current_trx_ts;

static RB_HEAD(hierarchy_index_tree, SelvaModify_HierarchyNode) hierarchy_index_head = RB_INITIALIZER();

static int Vector_BS_Compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const SelvaModify_HierarchyNode *a = *(const SelvaModify_HierarchyNode **)a_raw;
    const SelvaModify_HierarchyNode *b = *(const SelvaModify_HierarchyNode **)b_raw;

    return strncmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

static int SelvaModify_HierarchyNode_Compare(const SelvaModify_HierarchyNode *a, const SelvaModify_HierarchyNode *b) {
    return strncmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

#define timespec_cmp(tvp, uvp, cmp)             \
    (((tvp)->tv_sec == (uvp)->tv_sec)           \
        ? ((tvp)->tv_nsec cmp (uvp)->tv_nsec)   \
        : ((tvp)->tv_sec cmp (uvp)->tv_sec))

RB_PROTOTYPE_STATIC(hierarchy_index_tree, SelvaModify_HierarchyNode, _index_entry, SelvaModify_HierarchyNode_Compare);
RB_GENERATE_STATIC(hierarchy_index_tree, SelvaModify_HierarchyNode, _index_entry, SelvaModify_HierarchyNode_Compare);

static SelvaModify_HierarchyNode *SelvaModify_NewNode(Selva_NodeId id) {
    SelvaModify_HierarchyNode *node = RedisModule_Alloc(sizeof(SelvaModify_HierarchyNode));
    if (!node) {
        /* TODO Panic */
        return NULL;
    }

    memset(node, 0, sizeof(SelvaModify_HierarchyNode));
    Vector_Init(&node->parents, INITIAL_VECTOR_LEN, Vector_BS_Compare);
    Vector_Init(&node->children, INITIAL_VECTOR_LEN, Vector_BS_Compare);
    /* TODO Check errors /\ */

    memcpy(node->id, id, SELVA_NODE_ID_SIZE);

    return node;
}

static SelvaModify_HierarchyNode *findNode(const Selva_NodeId id) {
        SelvaModify_HierarchySearchFilter filter;

        memcpy(&filter.id, id, SELVA_NODE_ID_SIZE);
        return RB_FIND(hierarchy_index_tree, &hierarchy_index_head, (SelvaModify_HierarchyNode *)(&filter));
}

static int SelvaModify_CrossInsert(SelvaModify_HierarchyNode *node, enum SelvaModify_HierarchyNode_Relationship rel, size_t n, Selva_NodeId *nodes) {
    for (size_t i = 0; i < n; i++) {
        Selva_NodeId *adjacentId = nodes + i;
        SelvaModify_HierarchyNode *adjacent = findNode(*adjacentId);

        if (!adjacent) {
            /* TODO Panic: parent not found */
            continue;
        }

        if (rel == RELATIONSHIP_CHILD) {
            /* node is a children to adjacent */
            Vector_Insert(&node->parents, adjacent);
            Vector_Insert(&adjacent->children, node);
        } else {
            /* node is a parent to adjacent */
            Vector_Insert(&node->children, adjacent);
            Vector_Insert(&adjacent->parents, node);
        }
    }

    return 0;
}

int SelvaModify_SetHierarchy(Selva_NodeId id, size_t nr_parents, Selva_NodeId *parents, size_t nr_children, Selva_NodeId *children) {
    SelvaModify_HierarchyNode *node = SelvaModify_NewNode(id);

    if (!RB_INSERT(hierarchy_index_tree, &hierarchy_index_head, node)) {
        /* TODO Panic: the same id was already there */
    }

    /* TODO Error handling */
    SelvaModify_CrossInsert(node, RELATIONSHIP_CHILD, nr_parents, parents);
    SelvaModify_CrossInsert(node, RELATIONSHIP_PARENT, nr_children, children);

    return 0;
}

int SelvaModify_FindParents(Selva_NodeId id, Selva_NodeId **parents) {
    int nr = 0;
    SelvaModify_HierarchyNode *node = findNode(id);
    Selva_NodeId *plist = RedisModule_Alloc(sizeof(Selva_NodeId));

    if (!node) {
        return -1;
    }

    // TODO just the actual traversal is missing
    SelvaModify_HierarchyNode *parent;
    VECTOR_FOREACH(parent, &node->parents) {
        Selva_NodeId *newPlist = RedisModule_Realloc(plist, ++nr * sizeof(Selva_NodeId));
        if (!newPlist) {
            /* TODO Panic: alloc failed */
            RedisModule_Free(plist);
            return -1;
        }
        plist = newPlist;

        memcpy(plist + nr - 1, parent->id, sizeof(Selva_NodeId));
    }

    return 0;
}

int SelvaModify_FindChildren(Selva_NodeId id, Selva_NodeId **children) {
    return -1;
}
