#pragma once
#ifndef SELVA_MODIFY
#define SELVA_MODIFY

enum SelvaModify_ArgType {
  SELVA_MODIFY_ARG_VALUE = '0',
  SELVA_MODIFY_ARG_BASIC = '1'
};

enum SelvaModify_AsyncTask {
  SELVA_MODIFY_ASYNC_TASK_PUBLISH = '0',
  SELVA_MODIFY_ASYNC_TASK_INDEX = '1',
};

struct SelvaModify_Basic {
  char *$default;
  char *$value;
  char *$ref;
  char *$increment;
};

int SelvaModify_SendAsyncTask(enum SelvaModify_AsyncTask async_task_type, int payload_length, char *payload, uint8_t retries);

#endif /* SELVA_MODIFY */
