#pragma once
#ifndef _SELVA_
#define _SELVA_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SELVA_NODE_ID_SIZE      10ul /* Must be at least sizeof(void *) */
#define SELVA_NODE_TYPE_SIZE    2
#define ROOT_NODE_ID            "root\0\0\0\0\0\0"

#define SELVA_ALIASES_KEY       "___selva_aliases"

/**
 * Default Redis key name for Selva hierarchy.
 */
#define HIERARCHY_DEFAULT_KEY "___selva_hierarchy"

#define SELVA_ID_FIELD         "id"
#define SELVA_ALIASES_FIELD    "aliases"
#define SELVA_CREATED_AT_FIELD "createdAt"
#define SELVA_UPDATED_AT_FIELD "updatedAt"

/*
 * Defines for SelvaObject user meta
 */
#define SELVA_OBJECT_META_SUBTYPE_OBJECT 0
#define SELVA_OBJECT_META_SUBTYPE_RECORD 1
#define SELVA_OBJECT_META_SUBTYPE_TEXT 2

/**
 * Type for Selva NodeId.
 */
typedef char Selva_NodeId[SELVA_NODE_ID_SIZE];

typedef char Selva_NodeType[SELVA_NODE_TYPE_SIZE];

#define SELVA_SUBSCRIPTION_ID_SIZE 32
#define SELVA_SUBSCRIPTION_ID_STR_LEN (2 * SELVA_SUBSCRIPTION_ID_SIZE)

/**
 * Type for Selva subscription IDs.
 * SHA256 in binary.
 */
typedef unsigned char Selva_SubscriptionId[SELVA_SUBSCRIPTION_ID_SIZE];

typedef int32_t Selva_SubscriptionMarkerId;

#define SELVA_SUBSCRIPTION_MARKER_ID_MIN INT32_MIN

/**
 * Hierarchy traversal order.
 * Recognized by SelvaModify_TraverseHierarchy().
 */
enum SelvaModify_HierarchyTraversal {
    SELVA_HIERARCHY_TRAVERSAL_NONE, /*!< Do nothing. */
    SELVA_HIERARCHY_TRAVERSAL_NODE, /*!< Visit just the given node. */
    SELVA_HIERARCHY_TRAVERSAL_CHILDREN, /*!< Visit children of the given node. */
    SELVA_HIERARCHY_TRAVERSAL_PARENTS, /*!< Visit parents of the given node. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS, /*!< Visit ancestors of the given node using BFS. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS, /*!< Visit descendants of the given node using BFS. */
    SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS, /*!< Visit ancestors of the given node using DFS. */
    SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS, /*!< Visit descendants of the given node using DFS. */
    SELVA_HIERARCHY_TRAVERSAL_DFS_FULL, /*!< Full DFS traversal of the whole hierarchy. */
    SELVA_HIERARCHY_TRAVERSAL_REF, /*!< Visit nodes pointed by a ref field. */
    SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD, /*!< Traverse an edge field according to its constraints using BFS. */
};

size_t Selva_NodeIdLen(const Selva_NodeId nodeId);

static inline void Selva_NodeIdCpy(Selva_NodeId dest, const char *src) {
#if (defined(__GNUC__) || defined(__GNUG__)) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
    strncpy(dest, src, SELVA_NODE_ID_SIZE);
#if (defined(__GNUC__) || defined(__GNUG__)) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

#endif /* _SELVA_ */
