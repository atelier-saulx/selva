import SubscriptionManager from './index'
import { QuerySubscription } from '../../../lua/src/get/query/types'

const MAX_TIMEOUT = 10 * 60 * 60 * 1000 // 10 minutes

export function updateTimeout(subsManager: SubscriptionManager) {
  if (subsManager.refreshNowQueriesTimeout) {
    clearTimeout(subsManager.refreshNowQueriesTimeout)
    subsManager.refreshNowQueriesTimeout = undefined
  }

  const tm = Math.min(subsManager.nowBasedQueries.nextRefresh, MAX_TIMEOUT)
  subsManager.refreshNowQueriesTimeout = setTimeout(() => {
    const updates: Promise<void>[] = []

    const now = Date.now()
    while (subsManager.nowBasedQueries.queries[0].nextRefresh <= now) {
      const entry = subsManager.nowBasedQueries.queries.shift()
      updates.push(subsManager.sendUpdate(entry.subId))
    }

    Promise.all(updates)
      .catch(e => {
        console.error('Failed to update now queries', e)
      })
      .finally(() => {
        if (!subsManager.refreshNowQueriesTimeout) {
          if (subsManager.nowBasedQueries.queries.length) {
            subsManager.nowBasedQueries.nextRefresh =
              subsManager.nowBasedQueries.queries[0].nextRefresh
          } else {
            subsManager.nowBasedQueries.nextRefresh = MAX_TIMEOUT
          }

          updateTimeout(subsManager)
        }
      })
  }, tm)
}

export function updateQueries(
  subsManager: SubscriptionManager,
  entry: { subId: string; nextRefresh: number }
) {
  const nextRefresh = entry.nextRefresh

  if (!this.nowBasedQueries) {
    this.nowBasedQueries = {
      nextRefresh,
      queries: [entry]
    }
  } else {
    if (this.nowBasedQueries.nextRefresh > nextRefresh) {
      this.nowBasedQueries.nextRefresh = nextRefresh
    }

    // binary search insert
    let l = 0
    let r = this.nowBasedQueries.queries.length - 1
    let idx = 0
    while (l <= r) {
      idx = Math.floor((l + r) / 2)
      if (this.nowBasedQueries.queries[idx] < nextRefresh) {
        l = idx + 1
      } else if (this.nowBasedQueries.queries[idx] > nextRefresh) {
        r = idx - 1
      } else {
        break
      }
    }
    idx++

    this.nowBasedQueries.queries.splice(idx, 0, entry)
  }

  updateTimeout(subsManager)
}
