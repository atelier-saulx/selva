import { Subscription, SubscriptionManager } from './types'
import * as now from './now'

export function addRefreshMeta(
  subsmanager: SubscriptionManager,
  subscription: Subscription
) {
  const tree = subscription.tree

  if (tree && tree.___refreshAt) {
    subscription.refreshAt = tree.___refreshAt
    now.addSubscription(subsmanager, subscription)
  }
}

export function removeRefreshMeta(
  subsmanager: SubscriptionManager,
  subscription: Subscription
) {
  const tree = subscription.tree

  if (tree && tree.___refreshAt) {
    delete subscription.refreshAt
    now.removeSubscription(subsmanager, subscription)
  }
}
