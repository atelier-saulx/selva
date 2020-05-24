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
void SelvaModify_PreparePublishPayload(char *payload_str, char *id_str, size_t id_size, char *field_str, size_t field_size);

#endif /* SELVA_MODIFY */
