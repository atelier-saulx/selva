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
#include "hierarchy.h"

#define HIERARCHY_ENCODING_VERSION 0
#define INITIAL_VECTOR_LEN 2

static RedisModuleType *HierarchyType;

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

static void SelvaModify_DestroyNode(SelvaModify_HierarchyNode *node);
RB_PROTOTYPE_STATIC(hierarchy_index_tree, SelvaModify_HierarchyNode, _index_entry, SelvaModify_HierarchyNode_Compare)

static int SVector_BS_Compare(const void ** restrict a_raw, const void ** restrict b_raw) {
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

    RB_INIT(&hierarchy->index_head);
    SVector_Init(&hierarchy->heads, 1, SVector_BS_Compare);
    /* TODO Check errors /\ */

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

static SelvaModify_HierarchyNode *newNode(const Selva_NodeId id) {
    SelvaModify_HierarchyNode *node = RedisModule_Alloc(sizeof(SelvaModify_HierarchyNode));
    if (!node) {
        return NULL;
    };

    memset(node, 0, sizeof(SelvaModify_HierarchyNode));
    SVector_Init(&node->parents, INITIAL_VECTOR_LEN, SVector_BS_Compare);
    SVector_Init(&node->children, INITIAL_VECTOR_LEN, SVector_BS_Compare);
    /* TODO Check errors /\ */

    memcpy(node->id, id, SELVA_NODE_ID_SIZE);

    return node;
}

static void SelvaModify_DestroyNode(SelvaModify_HierarchyNode *node) {
    SVector_Destroy(&node->parents);
    SVector_Destroy(&node->children);
    RedisModule_Free(node);
}

static SelvaModify_HierarchyNode *findNode(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id) {
        SelvaModify_HierarchySearchFilter filter;

        memcpy(&filter.id, id, SELVA_NODE_ID_SIZE);
        return RB_FIND(hierarchy_index_tree, &hierarchy->index_head, (SelvaModify_HierarchyNode *)(&filter));
}

static int crossInsert(SelvaModify_Hierarchy *hierarchy, SelvaModify_HierarchyNode *node, enum SelvaModify_HierarchyNode_Relationship rel, size_t n, const Selva_NodeId *nodes) {
    for (size_t i = 0; i < n; i++) {
        SelvaModify_HierarchyNode *adjacent = findNode(hierarchy, nodes[i]);

        if (!adjacent) {
            /* TODO Panic: parent not found */
            continue;
        }

        if (rel == RELATIONSHIP_CHILD) {
            /* node is a children to adjacent */
            SVector_Insert(&node->parents, adjacent);
            SVector_Insert(&adjacent->children, node);
        } else {
            if (SVector_Size(&adjacent->parents) == 0) {
                /* No longer orphan */
                SVector_Remove(&hierarchy->heads, adjacent);
            }

            /* node is a parent to adjacent */
            SVector_Insert(&node->children, adjacent);
            SVector_Insert(&adjacent->parents, node);
        }
    }

    return 0;
}

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
            SVector_Insert(&hierarchy->heads, it);
        }
    }
    SVector_Clear(vec_a);
}

int SelvaModify_SetHierarchy(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    SelvaModify_HierarchyNode *node = findNode(hierarchy, id);
    int isNewNode = 0;

    if (!node) {
        node = newNode(id);
        if (!node) {
            return -1;
        }
        isNewNode = 1;
    }

    if (isNewNode) {
        if (RB_INSERT(hierarchy_index_tree, &hierarchy->index_head, node) != NULL) {
            /* TODO Panic: the same id was already there */
            SelvaModify_DestroyNode(node);

            return -1;
        }
    } else {
        /* Clear the existing node relationships */
        removeRelationships(hierarchy, node, RELATIONSHIP_PARENT);
        removeRelationships(hierarchy, node, RELATIONSHIP_CHILD);
    }

    if (nr_parents == 0) {
        /* This node is orphan */
        SVector_Insert(&hierarchy->heads, node);
    }

    /* Set relationship relative to other nodes */
    /* TODO Error handling */
    crossInsert(hierarchy, node, RELATIONSHIP_CHILD, nr_parents, parents);
    crossInsert(hierarchy, node, RELATIONSHIP_PARENT, nr_children, children);

    return 0;
}

int SelvaModify_AddHierarchy(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    /* TODO AddHierarchy */
    return -1;
}

int SelvaModify_DelHierarchy(
        SelvaModify_Hierarchy *hierarchy,
        const Selva_NodeId id,
        size_t nr_parents,
        const Selva_NodeId *parents,
        size_t nr_children,
        const Selva_NodeId *children) {
    /* TODO DelHierarchy */
    return -1;
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

int SelvaModify_GetHierarchyHeads(SelvaModify_Hierarchy *hierarchy, Selva_NodeId **res) {
    SelvaModify_HierarchyNode **it;
    Selva_NodeId *list = NodeList_New(1);
    int nr_nodes = 0;

    SVECTOR_FOREACH(it, &hierarchy->heads) {
        list = NodeList_Insert(list, (*it)->id, ++nr_nodes);
        if (!list) {
            return -1;
        }
    }

    *res = list;
    return nr_nodes;
}

static int dfs(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **res, enum SelvaModify_HierarchyNode_Relationship dir) {
    int nr_nodes = 0;
    size_t offset;

    switch (dir) {
    case RELATIONSHIP_PARENT:
        offset = offsetof(SelvaModify_HierarchyNode, parents);
        break;
    case RELATIONSHIP_CHILD:
        offset = offsetof(SelvaModify_HierarchyNode, children);
        break;
    default:
        return -1;
    }

    Trx_Begin(&hierarchy->current_trx);

    SelvaModify_HierarchyNode *head = findNode(hierarchy, id);
    if (!head) {
        return -1;
    }

    Selva_NodeId *list = NodeList_New(1);

    SVector stack;
    SVector_Init(&stack, 100, NULL);
    SVector_Insert(&stack, head);

    while (SVector_Size(&stack) > 0) {
        SelvaModify_HierarchyNode *node = SVector_Pop(&stack);

        if (!Trx_IsStamped(&hierarchy->current_trx, &node->visit_stamp)) {
            /* Mark node as visited and add it to the list of ancestors/descendants */
            Trx_Stamp(&hierarchy->current_trx, &node->visit_stamp);
            if (node != head) {
                list = NodeList_Insert(list, node->id, ++nr_nodes);
                if (!list) {
                    nr_nodes = -1;
                    goto err;
                }
            }

            /* Add parents/children to the stack of unvisited nodes */
            SelvaModify_HierarchyNode **it;
            const SVector *vec = (SVector *)((char *)node + offset);
            /* cppcheck-suppress internalAstError */
            SVECTOR_FOREACH(it, vec) {
                SVector_Insert(&stack, *it);
            }
        }
    }

    *res = list;
err:
    SVector_Destroy(&stack);

    return nr_nodes;
}

/**
 * Find ancestors of a node using DFS.
 */
int SelvaModify_FindAncestors(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **ancestors) {
    return dfs(hierarchy, id, ancestors, RELATIONSHIP_PARENT);
}

int SelvaModify_FindDescendants(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **descendants) {
    return dfs(hierarchy, id, descendants, RELATIONSHIP_CHILD);
}

void *HierarchyTypeRDBLoad(RedisModuleIO *io, int encver) {
    if (encver != HIERARCHY_ENCODING_VERSION) {
        /*
         * We should actually log an error here, or try to implement
         * the ability to load older versions of our data structure.
         */
        return NULL;
    }

    /* TODO Load RDB */
    return NULL;
}

void HierarchyTypeRDBSave(RedisModuleIO *io, void *value) {
    SelvaModify_Hierarchy *hierarchy = (SelvaModify_Hierarchy *)value;
    SelvaModify_HierarchyNode **head;
    SVector stack;

    SVector_Init(&stack, 100, NULL);
    Trx_Begin(&hierarchy->current_trx);

    SVECTOR_FOREACH(head, &hierarchy->heads) {
        SVector_Insert(&stack, *head);

        while (SVector_Size(&stack) > 0) {
            SelvaModify_HierarchyNode *node = SVector_Pop(&stack);

            if (!Trx_IsStamped(&hierarchy->current_trx, &node->visit_stamp)) {
                /* Mark node as visited */
                Trx_Stamp(&hierarchy->current_trx, &node->visit_stamp);

                /*
                 * Serialization format:
                 * NODE_ID | NR_CHILDREN | CHILD_ID_0,..
                 */
                RedisModule_SaveStringBuffer(io, node->id, SELVA_NODE_ID_SIZE);
                RedisModule_SaveUnsigned(io, SVector_Size(&node->children));

                /* Add parents to the stack of unvisited nodes */
                SelvaModify_HierarchyNode **itt;
                /* cppcheck-suppress internalAstError */
                SVECTOR_FOREACH(itt, &node->children) {
                    SelvaModify_HierarchyNode *it = *itt;

                    RedisModule_SaveStringBuffer(io, it->id, SELVA_NODE_ID_SIZE);
                    SVector_Insert(&stack, it);
                }
            }
        }
    }

    SVector_Destroy(&stack);
}

void HierarchyTypeAOFRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    /*
     * TODO AOF Rewrite
     * RedisModule_EmitAOF(aof,"HELLOTYPE.INSERT","sl",key,node->value);
     */
}

void HierarchyTypeFree(void *value) {
    SelvaModify_Hierarchy *hierarchy = (SelvaModify_Hierarchy *)value;

    SelvaModify_DestroyHierarchy(hierarchy);
}

int Hierarchy_OnLoad(RedisModuleCtx *ctx) {
    RedisModuleTypeMethods tm = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = HierarchyTypeRDBLoad,
        .rdb_save = HierarchyTypeRDBSave,
        .aof_rewrite = HierarchyTypeAOFRewrite,
        .free = HierarchyTypeFree,
    };

    HierarchyType = RedisModule_CreateDataType(ctx, "HierarchyType-AZ", HIERARCHY_ENCODING_VERSION, &tm);
    if (HierarchyType == NULL)
        return REDISMODULE_ERR;

    //if (RedisModule_CreateCommand(ctx, "selva.hierarchy.set", HelloTypeInsert_RedisCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR)
    //    return REDISMODULE_ERR;

    return REDISMODULE_OK;
}
