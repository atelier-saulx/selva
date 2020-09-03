#pragma once
#ifndef SELVA_MODIFY_H
#define SELVA_MODIFY_H

#include <stdbool.h>
#include <stdint.h>
#include "alias.h"
#include "async_task.h"

struct SelvaModify_Hierarchy;

enum SelvaModify_ArgType {
    SELVA_MODIFY_ARG_VALUE = '0',
    SELVA_MODIFY_ARG_DEFAULT = '2',
    SELVA_MODIFY_ARG_OP_INCREMENT = '4',
    SELVA_MODIFY_ARG_OP_SET = '5',
    SELVA_MODIFY_ARG_STRING_ARRAY = '6',
    SELVA_MODIFY_ARG_OP_DEL = '7',
};

struct SelvaModify_OpIncrement {
    int32_t index;
    int32_t $default;
    int32_t $increment;
};

struct SelvaModify_OpSet {
    int8_t is_reference;
    int8_t delete_all;

    // filled with multiple ids of length 10
    char *$add;
    size_t $add_len;

    // filled with multiple ids of length 10
    char *$delete;
    size_t $delete_len;

    // filled with multiple ids of length 10
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
    RedisModuleString *id,
    RedisModuleString *field,
    const char *field_str,
    size_t field_len,
    RedisModuleString *current_value,
    const char *current_value_str,
    size_t current_value_len,
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
