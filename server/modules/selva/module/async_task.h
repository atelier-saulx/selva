#pragma once
#ifndef SELVA_MODIFY_ASYNC_TASK
#define SELVA_MODIFY_ASYNC_TASK

#include "selva.h"

enum SelvaModify_AsyncEventType {
    SELVA_MODIFY_ASYNC_TASK_SUB_UPDATE,
};

struct SelvaModify_AsyncTask {
    enum SelvaModify_AsyncEventType type;

    union {
        struct {
            Selva_SubscriptionId sub_id;
        };
        struct {
            char id[SELVA_NODE_ID_SIZE];
            char *field_name;
            size_t field_name_len;
        };
    };
};

int SelvaModify_SendAsyncTask(const char *payload, size_t payload_len);
void SelvaModify_PublishSubscriptionUpdate(const Selva_SubscriptionId sub_id);

#endif /* SELVA_MODIFY_ASYNC_TASK */
