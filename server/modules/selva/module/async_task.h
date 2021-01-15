#pragma once
#ifndef SELVA_MODIFY_ASYNC_TASK
#define SELVA_MODIFY_ASYNC_TASK

#include "selva.h"

enum SelvaModify_AsyncEventType {
    SELVA_MODIFY_ASYNC_TASK_SUB_UPDATE,
    SELVA_MODIFY_ASYNC_TASK_SUB_TRIGGER,
};

struct SelvaModify_AsyncTask {
    enum SelvaModify_AsyncEventType type;

    union {
        /* SELVA_MODIFY_ASYNC_TASK_SUB_UPDATE */
        struct {
            Selva_SubscriptionId sub_id;
            Selva_SubscriptionMarkerId marker_id;
        } sub_update;
        /* SELVA_MODIFY_ASYNC_TASK_SUB_TRIGGER */
        struct {
            Selva_SubscriptionId sub_id;
            Selva_SubscriptionMarkerId marker_id;
            Selva_NodeId node_id;
        } sub_trigger;
    };
};

int SelvaModify_SendAsyncTask(const char *payload, size_t payload_len);
void SelvaModify_PublishSubscriptionUpdate(const Selva_SubscriptionId sub_id);
void SelvaModify_PublishSubscriptionTrigger(const Selva_SubscriptionId sub_id, const Selva_NodeId node_id);

#endif /* SELVA_MODIFY_ASYNC_TASK */
