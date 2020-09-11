#pragma once
#ifndef SELVA_MODIFY_SUBSCRIPTIONS
#define SELVA_MODIFY_SUBSCRIPTIONS

#include "selva.h"

/**
 * Hierarchy changed.
 */
#define SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY    0x0001

/**
 * Field changed.
 */
#define SELVA_SUBSCRIPTION_FLAG_CH_FIELD        0x0002

struct SelvaModify_Hierarchy;
struct SelvaModify_HierarchyMetadata;
struct hierarchy_subscriptions_tree;

/**
 * Selva subscription ID to hex string.
 */
char *Selva_SubscriptionId2str(char dest[SELVA_SUBSCRIPTION_ID_STR_LEN + 1], const Selva_SubscriptionId sub_id);

int Selva_SubscriptionStr2id(Selva_SubscriptionId dest, const char *src);

void Selva_DestroySubscriptions(struct SelvaModify_Hierarchy *hierarchy);
void Selva_DeleteSubscription(struct SelvaModify_Hierarchy *hierarchy, Selva_SubscriptionId sub_id);
void Selva_ClearAllSubscriptionMarkers(
        struct SelvaModify_Hierarchy *hierarchy,
        Selva_NodeId node_id,
        struct SelvaModify_HierarchyMetadata *metadata);
void Selva_HandleFieldChangeSubscriptions(const Selva_NodeId node_id, const struct SelvaModify_HierarchyMetadata *metadata, const char *field);

#endif /* SELVA_MODIFY_SUBSCRIPTIONS */
