/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef SELVA_MODIFY_H
#define SELVA_MODIFY_H

#include "alias.h"
#include "async_task.h"
#include "selva_object.h"

struct SelvaHierarchy;
struct SelvaObject;
struct selva_string;

enum SelvaModify_ArgType {
    SELVA_MODIFY_ARG_INVALID = '\0',
    /* Node object string field operations. */
    SELVA_MODIFY_ARG_DEFAULT_STRING = '2', /*!< Set a string value if unset. */
    SELVA_MODIFY_ARG_STRING = '0', /*!< Value is a string. */
    /* Node object numeric field operations. */
    SELVA_MODIFY_ARG_DEFAULT_LONGLONG = '8',
    SELVA_MODIFY_ARG_LONGLONG = '3', /*!< Value is a long long. */
    SELVA_MODIFY_ARG_DEFAULT_DOUBLE = '9',
    SELVA_MODIFY_ARG_DOUBLE = 'A', /*!< Value is a double. */
    SELVA_MODIFY_ARG_OP_INCREMENT = '4', /*!< Increment a long long value. */
    SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE = 'B', /*!< Increment a double value. */
    /* Node object set field operations. */
    SELVA_MODIFY_ARG_OP_SET = '5', /*!< Value is a struct SelvaModify_OpSet. */
    /* Node object array field operations. */
    SELVA_MODIFY_ARG_OP_ARRAY_PUSH = 'D', /*!< Set a new empty SelvaObject at the end of an array */
    SELVA_MODIFY_ARG_OP_ARRAY_INSERT = 'E', /*!< Set a new empty SelvaObject at the start of an array */
    SELVA_MODIFY_ARG_OP_ARRAY_REMOVE = 'F', /*!< Remove item in specified index from array */
    /* Node object operations. */
    SELVA_MODIFY_ARG_OP_DEL = '7', /*!< Delete field; value is a modifier. */
    SELVA_MODIFY_ARG_OP_OBJ_META = 'C', /*!< Set object user metadata. */
    /* Edge metadata ops. */
    SELVA_MODIFY_ARG_OP_EDGE_META = 'G', /*!< Modify edge field metadata. */
    /* Other ops. */
    SELVA_MODIFY_ARG_STRING_ARRAY = '6', /*!< Array of C-strings. */
};

struct SelvaModify_OpIncrement {
    int64_t $default;
    int64_t $increment;
};

struct SelvaModify_OpIncrementDouble {
    double $default;
    double $increment;
};

enum SelvaModify_OpSetType {
    SELVA_MODIFY_OP_SET_TYPE_CHAR = 0,
    SELVA_MODIFY_OP_SET_TYPE_REFERENCE = 1, /*!< Items are of size SELVA_NODE_ID_SIZE. */
    SELVA_MODIFY_OP_SET_TYPE_DOUBLE = 2,
    SELVA_MODIFY_OP_SET_TYPE_LONG_LONG = 3,
};

struct SelvaModify_OpSet {
    int8_t op_set_type; /*!< Set type. One of SELVA_MODIFY_OP_SET_TYPE_xxx. */
    int8_t delete_all; /*!< Delete all intems from the set. */
    uint16_t edge_constraint_id; /*!< Edge field constraint id when op_set_type is set to SELVA_MODIFY_OP_SET_TYPE_EDGE. */

    char *$add;
    size_t $add_len;

    char *$delete;
    size_t $delete_len;

    char *$value;
    size_t $value_len;
};

enum SelvaModify_OpEdgetMetaCode {
    SELVA_MODIFY_OP_EDGE_META_DEL = 0,
    SELVA_MODIFY_OP_EDGE_META_DEFAULT_STRING = 1,
    SELVA_MODIFY_OP_EDGE_META_STRING = 2,
    SELVA_MODIFY_OP_EDGE_META_DEFAULT_LONGLONG = 3,
    SELVA_MODIFY_OP_EDGE_META_LONGLONG = 4,
    SELVA_MODIFY_OP_EDGE_META_DEFAULT_DOUBLE = 5,
    SELVA_MODIFY_OP_EDGE_META_DOUBLE = 6,
};

struct SelvaModify_OpEdgeMeta {
    int8_t op_code; /*!< Edge field metadata op code. */
    int8_t delete_all; /*!< Delete all metadata from this edge field. */

    char dst_node_id[SELVA_NODE_ID_SIZE];

    char *meta_field_name_str;
    size_t meta_field_name_len;

    char *meta_field_value_str;
    size_t meta_field_value_len;
};

/**
 * Modify op arg handler status.
 */
enum selva_op_repl_state {
    SELVA_OP_REPL_STATE_UNCHANGED,  /*!< No changes, do not replicate, reply with OK or ERR. */
    SELVA_OP_REPL_STATE_UPDATED,    /*!< Value changed, replicate, reply with UPDATED */
    SELVA_OP_REPL_STATE_REPLICATE,  /*!< Value might have changed, replicate, reply with OK */
};

enum selva_op_repl_state SelvaModify_ModifyMetadata(
        struct SelvaObject *obj,
        const struct selva_string *field,
        const struct selva_string *value);

struct SelvaModify_OpSet *SelvaModify_OpSet_align(
        const struct selva_string *data);

/**
 * Modify a set.
 * @returns >= 0 number of changes; or < 0 Selva error
 */
int SelvaModify_ModifySet(
    struct SelvaHierarchy *hierarchy,
    const Selva_NodeId node_id,
    struct SelvaHierarchyNode *node,
    struct SelvaObject *obj,
    const struct selva_string *field,
    struct SelvaModify_OpSet *setOpts
);

int SelvaModify_ModifyDel(
    struct SelvaHierarchy *hierarchy,
    struct SelvaHierarchyNode *node,
    struct SelvaObject *obj,
    const struct selva_string *field
);

#endif /* SELVA_MODIFY_H */
