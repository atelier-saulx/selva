#pragma once
#ifndef SELVA_MODIFY
#define SELVA_MODIFY

enum SelvaModify_ArgType {
  SELVA_MODIFY_ARG_VALUE = '0',
  SELVA_MODIFY_ARG_INDEXED_VALUE = '1'
};

// struct SelvaModify_Basic {
//   char *$default;
//   char *$value;
//   char *$ref;
//   char *$increment;
// };

enum SelvaModify_AsyncTaskType {
  SELVA_MODIFY_ASYNC_TASK_PUBLISH = 0,
  SELVA_MODIFY_ASYNC_TASK_INDEX = 1
};

struct SelvaModify_AsyncTask {
  enum SelvaModify_AsyncTaskType type;
  char id[10];
  const char *fieldName;
  const char *value;
};

int SelvaModify_SendAsyncTask(int payload_size, char *payload, uint8_t retries);
void SelvaModify_PreparePublishPayload(char *payload_str, const char *id_str, size_t id_size, const char *field_str, size_t field_size);
void SelvaModify_PrepareValueIndexPayload(char *payload_str, const char *id_str, size_t id_size, const char *field_str, size_t field_size, const char *value_str, size_t value_size);

#endif /* SELVA_MODIFY */
