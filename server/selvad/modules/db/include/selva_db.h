/*
 * Copyright (c) 2022-2023 SAULX
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
#define SELVA_NODE_ID_SIZE      16ul /* Must be at least sizeof(void *) */
#define SELVA_PRI_NODE_ID       "16s"
/**
 * NodeId type prefix size.
 */
#define SELVA_NODE_TYPE_SIZE    2
#define SELVA_PRI_NODE_TYPE     "2s"
/**
 * NodeId of the root node.
 */
#define ROOT_NODE_ID            "root\0\0\0\0\0\0\0\0\0\0\0\0"
/**
 * An empty nodeId.
 */
#define EMPTY_NODE_ID           "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"

#define SELVA_ALIASES_KEY       "___selva_aliases"

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

#define SELVA_IS_TYPE_FIELD(_s, _len) \
    ((_len) == (sizeof(SELVA_TYPE_FIELD) - 1) && __builtin_memcmp((_s), SELVA_TYPE_FIELD, sizeof(SELVA_TYPE_FIELD) - 1))

/*
 * Defines for SelvaObject user meta
 */
#define SELVA_OBJECT_META_SUBTYPE_OBJECT 0
#define SELVA_OBJECT_META_SUBTYPE_RECORD 1
#define SELVA_OBJECT_META_SUBTYPE_TEXT 2
#define SELVA_OBJECT_META_SUBTYPE_TIMESERIES 3

struct selva_string;

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
 * Field protection modes.
 */
enum selva_field_prot_mode {
    /**
     * Allows writing to the field.
     */
    SELVA_FIELD_PROT_WRITE = 0x01,
    /**
     * Allows deleting the field.
     */
    SELVA_FIELD_PROT_DEL   = 0x02,
};

/**
 * Test if a field is protected.
 * E.g. if mode = SELVA_FIELD_PROT_WRITE and the function returns 1,
 * it means that the field can be written to.
 */
int selva_field_prot_check(const struct selva_string *s, enum selva_field_prot_mode mode);
int selva_field_prot_check_str(const char *field_str, size_t field_len, enum selva_field_prot_mode mode);

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
 * Copy selva_string into a nodeId buffer.
 */
int selva_string2node_id(Selva_NodeId nodeId, const struct selva_string *s);


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

int Selva_SubscriptionStr2id(Selva_SubscriptionId dest, const char *src, size_t len);

/**
 * Set if the database has been modified since last save.
 */
extern int selva_db_is_dirty;

#endif /* _SELVA_ */
