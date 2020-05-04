import SubscriptionManager from './subsManager'
import { Subscription } from './'
import addUpdate from './update/addUpdate'
const MAX_TIMEOUT = 10 * 60 * 60 * 1000 // 10 minutes

export function updateTimeout(subsManager: SubscriptionManager) {
  if (subsManager.refreshNowQueriesTimeout) {
    clearTimeout(subsManager.refreshNowQueriesTimeout)
    subsManager.refreshNowQueriesTimeout = undefined
  }

  if (!subsManager.refreshSubscriptions) {
    subsManager.refreshSubscriptions = {
      nextRefresh: MAX_TIMEOUT,
      subscriptions: []
    }
  }

  let tm = Math.min(
    subsManager.refreshSubscriptions.nextRefresh - Date.now() + 10,
    MAX_TIMEOUT
  )
  if (tm < 0) {
    tm = 0
  }

  subsManager.refreshNowQueriesTimeout = setTimeout(() => {
    const updates: Promise<void>[] = []

    if (!subsManager.refreshSubscriptions.subscriptions.length) {
      subsManager.refreshSubscriptions.nextRefresh = MAX_TIMEOUT
      return updateTimeout(subsManager)
    }

    const now = Date.now()
    while (
      subsManager.refreshSubscriptions.subscriptions.length &&
      subsManager.refreshSubscriptions.subscriptions[0].refreshAt <= now
    ) {
      const subscription = subsManager.refreshSubscriptions.subscriptions.shift()
      addUpdate(subsManager, subscription)
    }

    Promise.all(updates)
      .catch(e => {
        console.error('Failed to update now queries', e)
      })
      .finally(() => {
        if (subsManager.refreshSubscriptions.subscriptions.length) {
          subsManager.refreshSubscriptions.nextRefresh =
            subsManager.refreshSubscriptions.subscriptions[0].refreshAt
        } else {
          subsManager.refreshSubscriptions.nextRefresh =
            Date.now() + MAX_TIMEOUT
        }

        updateTimeout(subsManager)
      })
  }, tm)
}

export function removeSubscription(
  subsManager: SubscriptionManager,
  subscription: Subscription
) {
  const refreshSubs = subsManager.refreshSubscriptions.subscriptions
  for (let i = 0; i < refreshSubs.length; i++) {
    if (refreshSubs[i] === subscription) {
      refreshSubs.splice(i, 1)
      break
    }
  }
}

export function addSubscription(
  subsManager: SubscriptionManager,
  subscription: Subscription
) {
  const nextRefresh = subscription.refreshAt

  if (!subsManager.refreshSubscriptions) {
    subsManager.refreshSubscriptions = {
      nextRefresh,
      subscriptions: [subscription]
    }
  } else {
    if (subsManager.refreshSubscriptions.nextRefresh > nextRefresh) {
      subsManager.refreshSubscriptions.nextRefresh = nextRefresh
    }

    // binary search insert
    let l = 0
    let r = subsManager.refreshSubscriptions.subscriptions.length - 1
    let idx = 0
    while (l <= r) {
      idx = Math.floor((l + r) / 2)
      if (
        subsManager.refreshSubscriptions.subscriptions[idx].refreshAt <
        nextRefresh
      ) {
        l = idx + 1
      } else if (
        subsManager.refreshSubscriptions.subscriptions[idx].refreshAt >
        nextRefresh
      ) {
        r = idx - 1
      } else {
        break
      }
    }
    idx++

    subsManager.refreshSubscriptions.subscriptions.splice(idx, 0, subscription)
  }

  updateTimeout(subsManager)
}
