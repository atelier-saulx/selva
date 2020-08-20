#pragma once
#ifndef SELVA_MODIFY_ASYNC_TASK
#define SELVA_MODIFY_ASYNC_TASK

enum SelvaModify_AsyncEventType {
    SELVA_MODIFY_ASYNC_TASK_CREATED,
    SELVA_MODIFY_ASYNC_TASK_REMOVED,
    SELVA_MODIFY_ASYNC_TASK_UPDATE,
};

struct SelvaModify_AsyncTask {
    enum SelvaModify_AsyncEventType type;

    char id[10];

    const char *field_name;
    size_t field_name_len;

    const char *value;
    size_t value_len;
};

int SelvaModify_SendAsyncTask(int payload_len, const char *payload);
void SelvaModify_PreparePublishPayload_Created(char *payload_str, const char *id_str);
void SelvaModify_PreparePublishPayload_Update(char *payload_str, const char *id_str, const char *field_str, size_t field_len);

#endif /* SELVA_MODIFY_ASYNC_TASK */
