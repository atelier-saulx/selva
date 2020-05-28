#pragma once
#ifndef SELVA_MODIFY
#define SELVA_MODIFY

#include <stdbool.h>
#include "./async_task.h"

enum SelvaModify_ArgType {
  SELVA_MODIFY_ARG_VALUE = '0',
  SELVA_MODIFY_ARG_INDEXED_VALUE = '1',
  SELVA_MODIFY_ARG_DEFAULT = '2',
  SELVA_MODIFY_ARG_DEFAULT_INDEXED = '3',
  SELVA_MODIFY_ARG_OP_INCREMENT = '4',
  SELVA_MODIFY_ARG_OP_SET = '5'
};

struct SelvaModify_OpIncrement {
  int index;
  int $default;
  int $increment;
};

struct SelvaModify_OpSet {
  int is_reference;

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

static inline void SelvaModify_OpSet_align(struct SelvaModify_OpSet *op) {
  op->$add = (char *)((char *)op + sizeof(struct SelvaModify_OpSet));
  op->$delete = (char *)((char *)op + sizeof(struct SelvaModify_OpSet) + op->$add_len);
  op->$value = (char *)((char *)op + sizeof(struct SelvaModify_OpSet) + op->$add_len + op->$delete_len);
}

static inline void SelvaModify_Index(const char *id_str, size_t id_len, const char *field_str, size_t field_len, const char *value_str, size_t value_len) {
  int indexing_str_len =
    sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + field_len + value_len;
  char indexing_str[indexing_str_len];
  SelvaModify_PrepareValueIndexPayload(indexing_str, id_str, id_len, field_str, field_len,
      value_str, value_len);
  SelvaModify_SendAsyncTask(indexing_str_len, indexing_str);
}

#endif /* SELVA_MODIFY */
