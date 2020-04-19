import { Subscription } from '../'
import SubscriptionManager from '../subsManager'
import addUpdate from './addUpdate'

// clean this up more
let execBatch
let fieldsProgress = {}
let fieldsInBatch = []

type Contains = { $field: string; $value: string[] }

const createBatch = (subsManager: SubscriptionManager) => {
  const memberMemCache = subsManager.memberMemCache

  execBatch = subsManager.client.redis.redis.client.batch()
  process.nextTick(() => {
    execBatch.exec((err, d) => {
      if (err) {
        console.error(err)
      } else {
        d.forEach((m, i) => {
          if (!m) {
            m = []
          } else {
            console.log('cannot find members', fieldsInBatch[i])
          }
          const field = fieldsInBatch[i]
          const members = (memberMemCache[field] = {})
          m.forEach(v => (members[v] = true))
          const listeners = fieldsProgress[field]
          for (let i = 0; i < listeners.length - 1; i += 2) {
            const v = listeners[i + 1]
            if (members[v]) {
              const s = listeners[i]
              s.forEach(subs => {
                addUpdate(subsManager, subs)
              })
            }
          }
        })
      }
      fieldsProgress = {}
      fieldsInBatch = []
      execBatch = undefined
    })
  })
}

const addAncestorsToBatch = (
  subsManager: SubscriptionManager,
  subscriptions: Set<Subscription>,
  field: string,
  v: string
) => {
  if (!execBatch) {
    createBatch(subsManager)
  }
  if (!fieldsProgress[field]) {
    execBatch.zrange(field, 0, -1)
    fieldsInBatch.push(field)
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
  if (!execBatch) {
    createBatch(subsManager)
  }
  if (!fieldsProgress[field]) {
    execBatch.smembers(field)
    fieldsInBatch.push(field)
    fieldsProgress[field] = [subscriptions, v]
  } else {
    fieldsProgress[field].push(subscriptions, v)
  }
}

const membersContainsId = (
  subsManager: SubscriptionManager,
  id: string,
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
      const field = `${id}.ancestors`
      let f = memberMemCache[field]
      if (!f) {
        addAncestorsToBatch(subsManager, subscriptions, field, v)
      } else if (f[v]) {
        return true
      }
    }
  } else {
    for (let k = 0; k < value.length; k++) {
      const v = value[k]
      const field = `${id}.${m.$field}`
      let f = memberMemCache[field]
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
  id: string,
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
    const memberCheck =
      subManager.tree.___contains && subManager.tree.___contains[contains]

    if (memberCheck) {
      if (membersContainsId(subManager, id, <Contains>memberCheck, subs)) {
        subs.forEach(s => {
          addUpdate(subManager, s)
        })
      }
    }
    // ------
  }
}

export default contains
