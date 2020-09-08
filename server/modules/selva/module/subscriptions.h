#pragma once
#ifndef SELVA_MODIFY_SUBSCRIPTIONS
#define SELVA_MODIFY_SUBSCRIPTIONS

#include "selva.h"

typedef unsigned char Selva_SubscriptionId[32];

#define SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY    0x01
#define SELVA_SUBSCRIPTION_FLAG_CH_FIELD        0x02

struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyMetaData;
struct hierarchy_subscriptions_tree;

void Selva_DestroySubscriptions(struct SelvaModify_Hierarchy *hierarchy);
void Selva_DeleteSubscription(struct SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id);
void Selva_ClearAllSubscriptionMarkers(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetaData *metadata);

#endif /* SELVA_MODIFY_SUBSCRIPTIONS */
