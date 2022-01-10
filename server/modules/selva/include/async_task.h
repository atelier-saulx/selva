#pragma once
#ifndef SELVA_ASYNC_TASK_H
#define SELVA_ASYNC_TASK_H

#include "selva.h"

/**
 * Publish a subscription update.
 */
void SelvaModify_PublishSubscriptionUpdate(const Selva_SubscriptionId sub_id);

/**
 * Publish trigger update with a node_id.
 */
void SelvaModify_PublishSubscriptionTrigger(const Selva_SubscriptionId sub_id, const Selva_NodeId node_id);

#endif /* SELVA_ASYNC_TASK_H */
