#pragma once
#ifndef SELVA_MODIFY_H
#define SELVA_MODIFY_H

#include <stdbool.h>
#include <stdint.h>
#include "alias.h"
#include "async_task.h"
#include "selva_object.h"

struct RedisModuleCtx;
struct SelvaHierarchy;
struct SelvaObject;

enum SelvaModify_ArgType {
    SELVA_MODIFY_ARG_DEFAULT_STRING = '2', /*!< Set a string value if unset. */
    SELVA_MODIFY_ARG_STRING = '0', /*!< Value is a string. */
    SELVA_MODIFY_ARG_STRING_ARRAY = '6', /*!< Array of C-strings. */
    SELVA_MODIFY_ARG_DEFAULT_LONGLONG = '8',
    SELVA_MODIFY_ARG_LONGLONG = '3', /*!< Value is a long long. */
    SELVA_MODIFY_ARG_DEFAULT_DOUBLE = '9',
    SELVA_MODIFY_ARG_DOUBLE = 'A', /*!< Value is a double. */
    SELVA_MODIFY_ARG_OP_INCREMENT = '4', /*!< Increment a long long value. */
    SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE = 'B', /*!< Increment a double value. */
    SELVA_MODIFY_ARG_OP_SET = '5', /*!< Value is a struct SelvaModify_OpSet. */
    SELVA_MODIFY_ARG_OP_DEL = '7', /*!< Delete field; value is a modifier. */
    SELVA_MODIFY_ARG_OP_OBJ_META = 'C', /*!< Set object user metadata. */
    SELVA_MODIFY_ARG_OP_ARRAY_PUSH = 'D', /*!< Set a new empty SelvaObject at the end of an array */
    SELVA_MODIFY_ARG_OP_ARRAY_INSERT = 'E', /*!< Set a new empty SelvaObject at the start of an array */
    SELVA_MODIFY_ARG_OP_ARRAY_REMOVE = 'F', /*!< Remove item in specified index from array */
};

struct SelvaModify_OpIncrement {
    int64_t $default;
    int64_t $increment;
};

struct SelvaModify_OpIncrementDouble {
    double $default;
    double $increment;
};

#define SELVA_MODIFY_OP_SET_TYPE_CHAR       0
#define SELVA_MODIFY_OP_SET_TYPE_REFERENCE  1 /*!< Items are of size SELVA_NODE_ID_SIZE. */
#define SELVA_MODIFY_OP_SET_TYPE_DOUBLE     2
#define SELVA_MODIFY_OP_SET_TYPE_LONG_LONG  3

struct SelvaModify_OpSet {
    int8_t op_set_type; /*!< Set type. One of the SELVA_MODIFY_OP_SET_TYPE_xxx defines. */
    int8_t delete_all; /*!< Delete all intems from the set. */
    uint16_t edge_constraint_id; /*!< Edge field constraint id when op_set_type is set to SELVA_MODIFY_OP_SET_TYPE_EDGE. */

    char *$add;
    size_t $add_len;

    char *$delete;
    size_t $delete_len;

    char *$value;
    size_t $value_len;
};

/**
 * Modify a set.
 * @returns >= 0 number of changes; or < 0 Selva error
 */
int SelvaModify_ModifySet(
    struct RedisModuleCtx *ctx,
    struct SelvaHierarchy *hierarchy,
    struct SelvaObject *obj,
    const Selva_NodeId node_id,
    const struct RedisModuleString *field,
    struct SelvaModify_OpSet *setOpts
);

int SelvaModify_ModifyDel(
    struct RedisModuleCtx *ctx,
    struct SelvaHierarchy *hierarchy,
    struct SelvaObject *obj,
    const Selva_NodeId node_id,
    const struct RedisModuleString *field
);

#endif /* SELVA_MODIFY_H */
