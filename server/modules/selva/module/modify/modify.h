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

int SelvaModify_SendAsyncTask(int payload_size, char *payload, uint8_t retries);

#endif /* SELVA_MODIFY */
