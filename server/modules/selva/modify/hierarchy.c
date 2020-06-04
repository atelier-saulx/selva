#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "redismodule.h"
#include "cdefs.h"
#include "tree.h"
#include "svector.h"
#include "trx.h"
#include "hierarchy.h"

#define INITIAL_VECTOR_LEN 2

typedef struct SelvaModify_HierarchyNode {
    Selva_NodeId id;
    struct timespec visit_stamp;
    Vector parents;
    Vector children;
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
    struct hierarchy_index_tree head;
} SelvaModify_Hierarchy;

static void SelvaModify_DestroyNode(SelvaModify_HierarchyNode *node);
RB_PROTOTYPE_STATIC(hierarchy_index_tree, SelvaModify_HierarchyNode, _index_entry, SelvaModify_HierarchyNode_Compare)

static int Vector_BS_Compare(const void ** restrict a_raw, const void ** restrict b_raw) {
    const SelvaModify_HierarchyNode *a = *(const SelvaModify_HierarchyNode **)a_raw;
    const SelvaModify_HierarchyNode *b = *(const SelvaModify_HierarchyNode **)b_raw;

    return strncmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

static int SelvaModify_HierarchyNode_Compare(const SelvaModify_HierarchyNode *a, const SelvaModify_HierarchyNode *b) {
    return strncmp(a->id, b->id, SELVA_NODE_ID_SIZE);
}

RB_GENERATE_STATIC(hierarchy_index_tree, SelvaModify_HierarchyNode, _index_entry, SelvaModify_HierarchyNode_Compare)

SelvaModify_Hierarchy *SelvaModify_NewHierarchy(void) {
    SelvaModify_Hierarchy *hierarchy = RedisModule_Alloc(sizeof(SelvaModify_HierarchyNode));
    if (!hierarchy) {
        return NULL;
    }

    RB_INIT(&hierarchy->head);

    return hierarchy;
}

void SelvaModify_DestroyHierarchy(SelvaModify_Hierarchy *hierarchy) {
    SelvaModify_HierarchyNode *node;
    SelvaModify_HierarchyNode *next;

	for (node = RB_MIN(hierarchy_index_tree, &hierarchy->head); node != NULL; node = next) {
		next = RB_NEXT(hierarchy_index_tree, &hierarchy->head, node);
		RB_REMOVE(hierarchy_index_tree, &hierarchy->head, node);
        SelvaModify_DestroyNode(node);
    }

    RedisModule_Free(hierarchy);
}

void SelvaModify_StartHierarchyTrx(SelvaModify_Hierarchy *hierarchy) {
    Trx_Begin(&hierarchy->current_trx);
}

static SelvaModify_HierarchyNode *newNode(const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node = RedisModule_Alloc(sizeof(SelvaModify_HierarchyNode));
    if (!node) {
        return NULL;
    };

    memset(node, 0, sizeof(SelvaModify_HierarchyNode));
    Vector_Init(&node->parents, INITIAL_VECTOR_LEN, Vector_BS_Compare);
    Vector_Init(&node->children, INITIAL_VECTOR_LEN, Vector_BS_Compare);
    /* TODO Check errors /\ */

    memcpy(node->id, id, SELVA_NODE_ID_SIZE);

    return node;
}

static void SelvaModify_DestroyNode(SelvaModify_HierarchyNode *node) {
    Vector_Destroy(&node->parents);
    Vector_Destroy(&node->children);
    RedisModule_Free(node);
}

static SelvaModify_HierarchyNode *findNode(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
        SelvaModify_HierarchySearchFilter filter;

        memcpy(&filter.id, id, SELVA_NODE_ID_SIZE);
        return RB_FIND(hierarchy_index_tree, &hierarchy->head, (SelvaModify_HierarchyNode *)(&filter));
}

static int SelvaModify_CrossInsert(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node, enum SelvaModify_HierarchyNode_Relationship rel, size_t n, const Selva_NodeId *nodes) {
    for (size_t i = 0; i < n; i++) {
        const Selva_NodeId *adjacentId = nodes + i;

        printf("adj: %s\n", *adjacentId);
        SelvaModify_HierarchyNode *adjacent = findNode(hierarchy, *adjacentId);

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

// TODO The root node obviously doesn't have parents but should we disallow
// adding more than one node with no parents?
int SelvaModify_SetHierarchy(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, size_t nr_parents, const Selva_NodeId *parents, size_t nr_children, const Selva_NodeId *children) {
    SelvaModify_HierarchyNode *node = newNode(id);

    if (RB_INSERT(hierarchy_index_tree, &hierarchy->head, node) != NULL) {
        /* TODO Panic: the same id was already there */
        SelvaModify_DestroyNode(node);

        return -1;
    }

    /* TODO Error handling */
    SelvaModify_CrossInsert(hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents);
    SelvaModify_CrossInsert(hierarchy, node, RELATIONSHIP_PARENT, nr_children, children);

    return 0;
}

static Selva_NodeId *NodeList_New(int nr_nodes) {
    return RedisModule_Alloc(nr_nodes * sizeof(Selva_NodeId));
}

static Selva_NodeId *NodeList_Insert(Selva_NodeId *list, Selva_NodeId id, int nr_nodes) {
    Selva_NodeId *newList = RedisModule_Realloc(list, nr_nodes * sizeof(Selva_NodeId));
    if (!newList) {
        RedisModule_Free(list);
        return NULL;
    }

    memcpy(newList + nr_nodes - 1, id, sizeof(Selva_NodeId));

    return newList;
}

/*
 * procedure DFS-iterative(G, v) is
 *     let S be a stack
 *     S.push(v)
 *     while S is not empty do
 *         v = S.pop()
 *         if v is not labeled as discovered then
 *             label v as discovered
 *             for all edges from v to w in G.adjacentEdges(v) do
 *                 S.push(w)
 */

int SelvaModify_FindAncestors(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **ancestors) {
    int nr_nodes = 0;
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);

    if (!node) {
        return -1;
    }

    Vector stack;
    Vector_Init(&stack, 100, NULL);
    Selva_NodeId *list = NodeList_New(1);

    Vector_Insert(&stack, node);
    while (Vector_Size(&stack) > 0) {
        SelvaModify_HierarchyNode *v = Vector_Pop(&stack);

        printf("Checking %s\n", v->id);

        if (!Trx_IsStamped(&hierarchy->current_trx, &v->visit_stamp)) {

            /* Mark node as visited and add it to the list of ancestors */
            Trx_Stamp(&hierarchy->current_trx, &v->visit_stamp);
            list = NodeList_Insert(list, v->id, ++nr_nodes);
            if (!list) {
                nr_nodes = -1;
                goto err;
            }

            /* Add parents to the stack of unvisited nodes */
            SelvaModify_HierarchyNode *parent;
            VECTOR_FOREACH(parent, &node->parents) {
                Vector_Insert(&stack, parent);
            }
        }
    }

    *ancestors = list;
err:
    Vector_Destroy(&stack);

    return nr_nodes;
}

int SelvaModify_FindDescendants(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **descendants) {
    return -1;
}
