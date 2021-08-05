#pragma once
#ifndef _SELVA_
#define _SELVA_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SELVA_NODE_ID_SIZE      10ul /* Must be at least sizeof(void *) */
#define SELVA_NODE_TYPE_SIZE    2
#define ROOT_NODE_ID            "root\0\0\0\0\0\0"
#define EMPTY_NODE_ID           "\0\0\0\0\0\0\0\0\0\0"

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

/**
 * Type of Selva NodeType.
 */
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
 * Selva version.
 */
extern const char * const selva_version;

/**
 * Get the length of nodeId ignoring nul bytes at the end of the string.
 */
size_t Selva_NodeIdLen(const Selva_NodeId nodeId);

/**
 * Copy a node id of any length from src to a fixed length Selva_NodeId variable.
 */
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

/**
 * Initialize a string array from a node_id or node type string.
 */
#define SELVA_TYPE_INITIALIZER(nodeid_or_type) \
    { nodeid_or_type[0], nodeid_or_type[1] }

/**
 * Compare node types.
 */
static inline int Selva_CmpNodeType(const char t1[SELVA_NODE_TYPE_SIZE], const char t2[SELVA_NODE_TYPE_SIZE]) {
    return !(*(unsigned short *)t1 ^ *(unsigned short *)t2);
}

/**
 * Compare nodeId to type.
 */
static inline int Selva_CmpNodeIdType(const Selva_NodeId nodeId, const char type[SELVA_NODE_TYPE_SIZE]) {
    return Selva_CmpNodeType(nodeId, type);
}

/**
 * Selva subscription ID to hex string.
 */
char *Selva_SubscriptionId2str(char dest[SELVA_SUBSCRIPTION_ID_STR_LEN + 1], const Selva_SubscriptionId sub_id);

int Selva_SubscriptionStr2id(Selva_SubscriptionId dest, const char *src);

#endif /* _SELVA_ */
