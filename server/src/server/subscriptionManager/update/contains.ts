import { Subscription, SubscriptionManager } from '../types'
import addUpdate from './addUpdate'
import { RedisCommand } from '@saulx/selva'

let fieldsProgress = {}
let fieldsInQueue = []
let queueIsBeingDrained = false
let queue: RedisCommand[] = []

type Contains = { $field: string; $value: string[] }

const drainQueue = (subsManager: SubscriptionManager) => {
  const { memberMemCache, client, selector } = subsManager
  const redis = client.redis
  queueIsBeingDrained = true
  process.nextTick(() => {
    let cnt = queue.length
    const q = queue
    const fieldsInProgressNow = fieldsProgress
    for (let i = 0; i < q.length; i++) {
      const command = q[i]
      const field = fieldsInQueue[i]
      // making a batch fn is nice for optmizations (for later!)
      command.resolve = m => {
        cnt--
        if (!m) m = []
        if (memberMemCache[field]) {
          subsManager.memberMemCacheSize++
        }
        const members = (memberMemCache[field] = {})
        m.forEach(v => (members[v] = true))
        const listeners = fieldsInProgressNow[field]
        for (let i = 0; i < listeners.length - 1; i += 2) {
          const v = listeners[i + 1]
          if (members[v]) {
            const s = listeners[i]
            s.forEach(subs => {
              addUpdate(subsManager, subs)
            })
          }
        }
        if (cnt === 0) {
          queueIsBeingDrained = false
        }
      }
      redis.addCommandToQueue(command, selector)
    }
    fieldsProgress = {}
    fieldsInQueue = []
    queue = []
  })
}

const addAncestorsToBatch = (
  subsManager: SubscriptionManager,
  subscriptions: Set<Subscription>,
  field: string,
  v: string
) => {
  if (!queueIsBeingDrained) {
    drainQueue(subsManager)
  }
  if (!fieldsProgress[field]) {
    queue.push({
      command: 'zrange',
      args: [field, 0, -1]
    })
    fieldsInQueue.push(field)
    fieldsProgress[field] = [subscriptions, v]
  } else {
    fieldsProgress[field].push(subscriptions, v)
  }
}

const addMembersToBatch = (
  subsManager: SubscriptionManager,
  subscriptions: Set<Subscription>,
  field: string,
  v: string
) => {
  if (!queueIsBeingDrained) {
    drainQueue(subsManager)
  }
  if (!fieldsProgress[field]) {
    queue.push({
      command: 'smembers',
      args: [field]
    })
    fieldsInQueue.push(field)
    fieldsProgress[field] = [subscriptions, v]
  } else {
    fieldsProgress[field].push(subscriptions, v)
  }
}

const membersContainsId = (
  subsManager: SubscriptionManager,
  context: { id: string; db: string },
  m: Contains,
  subscriptions: Set<Subscription>
): boolean => {
  const value = m.$value

  const memberMemCache = subsManager.memberMemCache
  if (m.$field === 'ancestors') {
    for (let k = 0; k < value.length; k++) {
      const v = value[k]
      if (v === 'root') {
        return true
      }
      const field = `${context.id}.ancestors`
      let f = memberMemCache[context.db][field]
      if (!f) {
        addAncestorsToBatch(subsManager, subscriptions, field, v)
      } else if (f[v]) {
        return true
      }
    }
  } else {
    for (let k = 0; k < value.length; k++) {
      const v = value[k]
      const field = `${context.id}.${m.$field}`
      let f = memberMemCache[context.db][field]
      if (!f) {
        addMembersToBatch(subsManager, subscriptions, field, v)
      } else if (f[v]) {
        return true
      }
    }
  }
  return false
}

const contains = (
  subManager: SubscriptionManager,
  contains: string,
  context: { id: string; db: string },
  subs: Set<Subscription>
) => {
  let inProgress = true
  for (const sub of subs.values()) {
    if (!sub.inProgress) {
      inProgress = false
      break
    }
  }
  if (!inProgress) {
    console.log('CHECKING', context, subManager.tree)
    const memberCheck =
      subManager.tree[context.db].___contains &&
      subManager.tree[context.db].___contains[contains]
    if (memberCheck) {
      if (membersContainsId(subManager, context, <Contains>memberCheck, subs)) {
        subs.forEach(s => {
          addUpdate(subManager, s)
        })
      }
    }
  }
}

export default contains
