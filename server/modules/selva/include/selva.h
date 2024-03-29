/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _SELVA_
#define _SELVA_

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "cdefs.h"

/**
 * Export a function.
 */
#define SELVA_EXPORT __attribute__((__visibility__("default")))

/**
 * NodeId size including the type prefix.
 */
#define SELVA_NODE_ID_SIZE      10ul /* Must be at least sizeof(void *) */
/**
 * NodeId type prefix size.
 */
#define SELVA_NODE_TYPE_SIZE    2
/**
 * NodeId of the root node.
 */
#define ROOT_NODE_ID            "root\0\0\0\0\0\0"
/**
 * An empty nodeId.
 */
#define EMPTY_NODE_ID           "\0\0\0\0\0\0\0\0\0\0"

#define SELVA_ALIASES_KEY       "___selva_aliases"

/**
 * Default Redis key name for Selva hierarchy.
 */
#define HIERARCHY_DEFAULT_KEY "___selva_hierarchy"

/**
 * Reserved field names.
 * @addtogroup selva_reserved_field_names
 * @{
 */
#define SELVA_ID_FIELD          "id"
#define SELVA_TYPE_FIELD        "type"
#define SELVA_ALIASES_FIELD     "aliases"
#define SELVA_PARENTS_FIELD     "parents"
#define SELVA_CHILDREN_FIELD    "children"
#define SELVA_ANCESTORS_FIELD   "ancestors"
#define SELVA_DESCENDANTS_FIELD "descendants"
#define SELVA_CREATED_AT_FIELD  "createdAt"
#define SELVA_UPDATED_AT_FIELD  "updatedAt"
/**
 * @}
 */

/*
 * Defines for SelvaObject user meta
 */
#define SELVA_OBJECT_META_SUBTYPE_OBJECT 0
#define SELVA_OBJECT_META_SUBTYPE_RECORD 1
#define SELVA_OBJECT_META_SUBTYPE_TEXT 2
#define SELVA_OBJECT_META_SUBTYPE_TIMESERIES 3

struct RedisModuleString;

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

/**
 * Type for Selva subscription marker id.
 */
typedef int64_t Selva_SubscriptionMarkerId;

#define PRImrkId PRId64

/**
 * Selva version.
 */
extern const char * const selva_version;

/**
 * Get the length of nodeId ignoring nul bytes at the end of the string.
 */
__purefn size_t Selva_NodeIdLen(const Selva_NodeId nodeId);

/**
 * Copy a node id of any length from src to a fixed length Selva_NodeId variable.
 */
static inline void Selva_NodeIdCpy(Selva_NodeId dest, const char *src) {
#if (defined(__GNUC__) || defined(__GNUG__)) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
    /* Note that strncpy() will handle nul padding. */
    strncpy(dest, src, SELVA_NODE_ID_SIZE);
#if (defined(__GNUC__) || defined(__GNUG__)) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

/**
 * Copy RedisModuleString into a nodeId buffer.
 */
int Selva_RMString2NodeId(Selva_NodeId nodeId, const struct RedisModuleString *rms);


/**
 * Initialize a string array from a node_id or node type string.
 */
#define SELVA_TYPE_INITIALIZER(nodeid_or_type) \
    { nodeid_or_type[0], nodeid_or_type[1] }

/**
 * Compare node types.
 */
static inline int Selva_CmpNodeType(const char t1[SELVA_NODE_TYPE_SIZE], const char t2[SELVA_NODE_TYPE_SIZE]) {
#if SELVA_NODE_TYPE_SIZE == 2
    unsigned short a, b;
#elif SELVA_NODE_TYPE_SIZE == 4
    unsigned int a, b;
#else
#error Unsupported SELVA_NODE_TYPE_SIZE
#endif

    _Static_assert(SELVA_NODE_TYPE_SIZE == sizeof(a), "type size matches the cmp variable");

    memcpy(&a, t1, SELVA_NODE_TYPE_SIZE);
    memcpy(&b, t2, SELVA_NODE_TYPE_SIZE);

    return a - b;
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

#include "_selva_log.h"
#include "_selva_errors.h"

#endif /* _SELVA_ */
