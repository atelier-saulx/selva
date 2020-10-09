#pragma once
#ifndef SELVA_MODIFY_H
#define SELVA_MODIFY_H

#include <stdbool.h>
#include <stdint.h>
#include "alias.h"
#include "async_task.h"

struct SelvaModify_Hierarchy;

enum SelvaModify_ArgType {
    SELVA_MODIFY_ARG_VALUE = '0', /*!< Value is a string. */
    SELVA_MODIFY_ARG_DEFAULT = '2', /*!< Set a string value if unset. */
    SELVA_MODIFY_ARG_OP_INCREMENT = '4', /*!< Increment long long. */
    SELVA_MODIFY_ARG_OP_SET = '5', /*!< Value is a struct SelvaModify_OpSet. */
    SELVA_MODIFY_ARG_STRING_ARRAY = '6', /*!< Array of C-strings. */
    SELVA_MODIFY_ARG_OP_DEL = '7', /*!< Delete field; value is a modifier. */
};

struct SelvaModify_OpIncrement {
    int32_t index;
    int32_t $default;
    int32_t $increment;
};

struct SelvaModify_OpSet {
    int8_t is_reference; /*!< If set then the arrays are of len SELVA_NODE_ID_SIZE. */
    int8_t delete_all; /*!< Delete all intems from the set. */

    char *$add;
    size_t $add_len;

    char *$delete;
    size_t $delete_len;

    char *$value;
    size_t $value_len;
};

static inline struct SelvaModify_OpSet *SelvaModify_OpSet_align(RedisModuleString *data) {
    TO_STR(data);
    struct SelvaModify_OpSet *op;

    if (data_len < sizeof(struct SelvaModify_OpSet)) {
        return NULL;
    }

    op = (struct SelvaModify_OpSet *)data_str;
    if (data_len < sizeof(struct SelvaModify_OpSet) + op->$add_len + op->$delete_len + op->$value_len) {
        return NULL;
    }

    op->$add = op->$add ? (char *)((char *)op + sizeof(*op)) : NULL;
    op->$delete = op->$delete ? (char *)((char *)op + sizeof(*op) + op->$add_len) : NULL;
    op->$value = op->$value ? (char *)((char *)op + sizeof(*op) + op->$add_len + op->$delete_len) : NULL;

    return op;
}

RedisModuleKey *SelvaModify_OpenSet(
        RedisModuleCtx *ctx,
        const char *id_str, size_t id_len,
        const char *field_str);

int SelvaModify_ModifySet(
    RedisModuleCtx *ctx,
    struct SelvaModify_Hierarchy *hierarchy,
    RedisModuleKey *id_key,
    RedisModuleString *id,
    RedisModuleString *field,
    struct SelvaModify_OpSet *setOpts
);

void SelvaModify_ModifyIncrement(
    RedisModuleCtx *ctx,
    RedisModuleKey *id_key,
    RedisModuleString *field,
    RedisModuleString *current_value,
    struct SelvaModify_OpIncrement *incrementOpts
);

int SelvaModify_ModifyDel(
    RedisModuleCtx *ctx,
    struct SelvaModify_Hierarchy *hierarchy,
    RedisModuleKey *id_key,
    RedisModuleString *id,
    RedisModuleString *field,
    const char *value_str
);

#endif /* SELVA_MODIFY_H */
