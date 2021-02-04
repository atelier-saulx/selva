#pragma once
#ifndef SELVA_MODIFY_H
#define SELVA_MODIFY_H

#include <stdbool.h>
#include <stdint.h>
#include "alias.h"
#include "async_task.h"
#include "selva_object.h"

struct RedisModuleCtx;
struct SelvaModify_Hierarchy;
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

    char *$add;
    size_t $add_len;

    char *$delete;
    size_t $delete_len;

    char *$value;
    size_t $value_len;
};

static inline struct SelvaModify_OpSet *SelvaModify_OpSet_align(struct RedisModuleString *data) {
    TO_STR(data);
    struct SelvaModify_OpSet *op;

    if (!data_str && data_len < sizeof(struct SelvaModify_OpSet)) {
        return NULL;
    }

    op = (struct SelvaModify_OpSet *)data_str;
    op->$add    = op->$add    ? (char *)((char *)op + (ptrdiff_t)op->$add)    : NULL;
    op->$delete = op->$delete ? (char *)((char *)op + (ptrdiff_t)op->$delete) : NULL;
    op->$value  = op->$value  ? (char *)((char *)op + (ptrdiff_t)op->$value)  : NULL;

    if ((ptrdiff_t)op->$add + op->$add_len > (ptrdiff_t)op + data_len ||
        (ptrdiff_t)op->$delete + op->$delete_len > (ptrdiff_t)op + data_len ||
        (ptrdiff_t)op->$value + op->$value_len > (ptrdiff_t)op + data_len) {
        return NULL;
    }

    return op;
}

/**
 * Modify a set.
 * @returns >= 0 number of changes; or < 0 Selva error
 */
int SelvaModify_ModifySet(
    struct RedisModuleCtx *ctx,
    struct SelvaModify_Hierarchy *hierarchy,
    struct SelvaObject *obj,
    struct RedisModuleString *id,
    struct RedisModuleString *field,
    struct SelvaModify_OpSet *setOpts
);

void SelvaModify_ModifyIncrement(
    struct SelvaObject *obj,
    struct RedisModuleString *field,
    enum SelvaObjectType old_type,
    struct SelvaModify_OpIncrement *incrementOpts
);

void SelvaModify_ModifyIncrementDouble(
    RedisModuleCtx *ctx,
    struct SelvaObject *obj,
    RedisModuleString *field,
    enum SelvaObjectType old_type,
    struct SelvaModify_OpIncrementDouble *incrementOpts
);

int SelvaModify_ModifyDel(
    struct RedisModuleCtx *ctx,
    struct SelvaModify_Hierarchy *hierarchy,
    struct SelvaObject *obj,
    struct RedisModuleString *id,
    struct RedisModuleString *field
);

#endif /* SELVA_MODIFY_H */
