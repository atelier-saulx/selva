#pragma once
#ifndef SELVA_MODIFY
#define SELVA_MODIFY

#include <stdbool.h>
#include "./async_task.h"

struct SelvaModify_Hierarchy;

enum SelvaModify_ArgType {
    SELVA_MODIFY_ARG_VALUE = '0',
    SELVA_MODIFY_ARG_INDEXED_VALUE = '1',
    SELVA_MODIFY_ARG_DEFAULT = '2',
    SELVA_MODIFY_ARG_DEFAULT_INDEXED = '3',
    SELVA_MODIFY_ARG_OP_INCREMENT = '4',
    SELVA_MODIFY_ARG_OP_SET = '5',
    SELVA_MODIFY_ARG_STRING_ARRAY = '6',
};

struct SelvaModify_OpIncrement {
    int index;
    int $default;
    int $increment;
};

struct SelvaModify_OpSet {
    int8_t is_reference;

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

RedisModuleKey *open_aliases_key(RedisModuleCtx *ctx);

static inline void SelvaModify_Index(const char *id_str, size_t id_len, const char *field_str, size_t field_len, const char *value_str, size_t value_len) {
    int indexing_str_len =
        sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + field_len + value_len;
    char indexing_str[indexing_str_len];
    SelvaModify_PrepareValueIndexPayload(indexing_str, id_str, id_len, field_str, field_len,
            value_str, value_len);
    SelvaModify_SendAsyncTask(indexing_str_len, indexing_str);
}

static inline void SelvaModify_Publish(const char *id_str, size_t id_len, const char *field_str, size_t field_len) {
    int payload_len = sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + field_len;
    char payload_str[payload_len];

    SelvaModify_PreparePublishPayload(payload_str, id_str, id_len, field_str, field_len);
    SelvaModify_SendAsyncTask(payload_len, payload_str);
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

#endif /* SELVA_MODIFY */
