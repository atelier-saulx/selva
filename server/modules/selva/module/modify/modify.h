#pragma once
#ifndef SELVA_MODIFY
#define SELVA_MODIFY

enum SelvaModify_ArgType {
  SELVA_MODIFY_ARG_VALUE = '0',
  SELVA_MODIFY_ARG_INDEXED_VALUE = '1',
  SELVA_MODIFY_ARG_DEFAULT = '2',
  SELVA_MODIFY_ARG_DEFAULT_INDEXED = '3',
  SELVA_MODIFY_ARG_OP_INCREMENT = '4',
  SELVA_MODIFY_ARG_OP_REFERENCES = '5'
};

struct SelvaModify_OpIncrement {
  // boolean indexed;

  char *$default;
  size_t $default_len;

  char *$increment;
  size_t $increment_len;
};


struct SelvaModify_OpReferences {
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


#endif /* SELVA_MODIFY */
