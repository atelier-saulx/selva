import SubscriptionManager from './index'

const MAX_TIMEOUT = 10 * 60 * 60 * 1000 // 10 minutes

export function updateTimeout(subsManager: SubscriptionManager) {
  if (subsManager.refreshNowQueriesTimeout) {
    clearTimeout(subsManager.refreshNowQueriesTimeout)
    subsManager.refreshNowQueriesTimeout = undefined
  }

  if (!subsManager.nowBasedQueries) {
    subsManager.nowBasedQueries = { nextRefresh: MAX_TIMEOUT, queries: [] }
  }

  let tm = Math.min(
    subsManager.nowBasedQueries.nextRefresh - Date.now() + 10,
    MAX_TIMEOUT
  )
  if (tm < 0) {
    tm = 0
  }

  subsManager.refreshNowQueriesTimeout = setTimeout(() => {
    const updates: Promise<void>[] = []

    if (!subsManager.nowBasedQueries.queries.length) {
      subsManager.nowBasedQueries.nextRefresh = MAX_TIMEOUT
      return updateTimeout(subsManager)
    }

    const now = Date.now()
    while (
      subsManager.nowBasedQueries.queries.length &&
      subsManager.nowBasedQueries.queries[0].nextRefresh <= now
    ) {
      const entry = subsManager.nowBasedQueries.queries.shift()
      updates.push(subsManager.sendUpdate(entry.subId))
    }

    Promise.all(updates)
      .catch(e => {
        console.error('Failed to update now queries', e)
      })
      .finally(() => {
        if (subsManager.nowBasedQueries.queries.length) {
          subsManager.nowBasedQueries.nextRefresh =
            subsManager.nowBasedQueries.queries[0].nextRefresh
        } else {
          subsManager.nowBasedQueries.nextRefresh = Date.now() + MAX_TIMEOUT
        }

        updateTimeout(subsManager)
      })
  }, tm)
}

export function updateQueries(
  subsManager: SubscriptionManager,
  entry: { subId: string; nextRefresh: number }
) {
  const nextRefresh = entry.nextRefresh

  if (!subsManager.nowBasedQueries) {
    subsManager.nowBasedQueries = {
      nextRefresh,
      queries: [entry]
    }
  } else {
    if (subsManager.nowBasedQueries.nextRefresh > nextRefresh) {
      subsManager.nowBasedQueries.nextRefresh = nextRefresh
    }

    // binary search insert
    let l = 0
    let r = subsManager.nowBasedQueries.queries.length - 1
    let idx = 0
    while (l <= r) {
      idx = Math.floor((l + r) / 2)
      if (subsManager.nowBasedQueries.queries[idx].nextRefresh < nextRefresh) {
        l = idx + 1
      } else if (
        subsManager.nowBasedQueries.queries[idx].nextRefresh > nextRefresh
      ) {
        r = idx - 1
      } else {
        break
      }
    }
    idx++

    subsManager.nowBasedQueries.queries.splice(idx, 0, entry)
  }

  updateTimeout(subsManager)
}
