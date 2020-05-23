#pragma once
#ifndef SELVA_MODIFY
#define SELVA_MODIFY

enum SelvaModify_ArgType {
  SELVA_MODIFY_ARG_VALUE = '0',
  SELVA_MODIFY_ARG_BASIC = '1'
};

struct SelvaModify_Basic {
  char *$default;
  char *$value;
  char *$ref;
  char *$increment;
};

#endif /* SELVA_MODIFY */
