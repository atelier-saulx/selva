#pragma once
#ifndef _SELVA_
#define _SELVA_

#include <stdint.h>

#define SELVA_NODE_ID_SIZE      10ul
#define SELVA_NODE_TYPE_SIZE    2
#define ROOT_NODE_ID            "root\0\0\0\0\0\0"

/**
 * Type for Selva NodeId.
 */
typedef char Selva_NodeId[SELVA_NODE_ID_SIZE];

#define SELVA_SUBSCRIPTION_ID_SIZE 32
#define SELVA_SUBSCRIPTION_ID_STR_LEN (2 * SELVA_SUBSCRIPTION_ID_SIZE)

/**
 * Type for Selva subscription IDs.
 * SHA256 in binary.
 */
typedef unsigned char Selva_SubscriptionId[SELVA_SUBSCRIPTION_ID_SIZE];

typedef int32_t Selva_SubscriptionMarkerId;

/**
 * Hierarchy traversal order.
 * Recognized by SelvaModify_TraverseHierarchy().
 */
enum SelvaModify_HierarchyTraversal {
    SELVA_HIERARCHY_TRAVERSAL_NONE,
    SELVA_HIERARCHY_TRAVERSAL_NODE,
    SELVA_HIERARCHY_TRAVERSAL_CHILDREN,
    SELVA_HIERARCHY_TRAVERSAL_PARENTS,
    SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS,
    SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS,
    SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS,
    SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS,
    SELVA_HIERARCHY_TRAVERSAL_DFS_FULL,
};

size_t Selva_NodeIdLen(Selva_NodeId nodeId);

#endif /* _SELVA_ */
