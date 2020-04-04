import SubscriptionManager, { QuerySubscription } from './index'
import { Multi } from 'redis'

let memberMemCache = {}
let batchUpdates: string[] = []
let inProgress: boolean = false
let execBatch: Multi | undefined
let fieldsProgress: Record<string, string[]> = {}
let fieldsInBatch: string[] = []

const addToUpdateQueue = (subsManager: SubscriptionManager, key: string) => {
  if (!subsManager.inProgress[key]) {
    subsManager.inProgress[key] = true
    batchUpdates.push(key)
    if (!inProgress) {
      inProgress = true
      // want to manage the time update based on amount of things
      setTimeout(
        () => {
          // maybe check amount and slowly drain
          console.log('QUERIES TO EXEC', batchUpdates.length)
          // check size for drainage
          for (let i = 0; i < batchUpdates.length; i++) {
            const key = batchUpdates[i]
            subsManager.sendUpdate(key).catch(err => {
              console.error(`Error query update from subscription ${key}`, err)
            })
          }
          batchUpdates = []
          inProgress = false
          subsManager.cleanUpProgress()
          memberMemCache = {}
        },
        subsManager.incomingCount > 15000
          ? 1500
          : subsManager.incomingCount > 1000
          ? 1000
          : subsManager.incomingCount > 500
          ? 500
          : 100
      )
    }
  }
}

const createBatch = (subsManager: SubscriptionManager) => {
  execBatch = subsManager.client.redis.redis.client.batch()
  process.nextTick(() => {
    execBatch.exec((err, d) => {
      if (err) {
        console.error(err)
      } else {
        d.forEach((m, i) => {
          const field = fieldsInBatch[i]
          const members = (memberMemCache[field] = {})
          m.forEach(v => (members[v] = true))
          const listeners = fieldsProgress[field]
          for (let i = 0; i < listeners.length - 1; i += 2) {
            const v = listeners[i + 1]
            if (members[v]) {
              addToUpdateQueue(subsManager, listeners[i])
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
  key: string,
  field: string,
  v: string
) => {
  if (!execBatch) {
    createBatch(subsManager)
  }
  if (!fieldsProgress[field]) {
    execBatch.zrange(field, 0, -1)
    fieldsInBatch.push(field)
    fieldsProgress[field] = [key, v]
  } else {
    fieldsProgress[field].push(key, v)
  }
}

const addMembersToBatch = (
  subsManager: SubscriptionManager,
  key: string,
  field: string,
  v: string
) => {
  if (!execBatch) {
    createBatch(subsManager)
  }
  if (!fieldsProgress[field]) {
    execBatch.smembers(field)
    fieldsInBatch.push(field)
    fieldsProgress[field] = [key, v]
  } else {
    fieldsProgress[field].push(key, v)
  }
}

const membersContainsId = (
  subsManager: SubscriptionManager,
  id: string,
  item: QuerySubscription,
  key: string
): boolean => {
  const member = item.member
  for (let j = 0; j < member.length; j++) {
    const m = member[j]
    const value = m.$value
    if (m.$field === 'ancestors') {
      for (let k = 0; k < value.length; k++) {
        const v = value[k]
        if (v === 'root') {
          return true
        }
        const field = `${id}.ancestors`
        let f = memberMemCache[field]
        if (!f) {
          addAncestorsToBatch(subsManager, key, field, v)
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
          addMembersToBatch(subsManager, key, field, v)
        } else if (f[v]) {
          return true
        }
      }
    }
  }
  return false
}

const handleQuery = (
  subsManager: SubscriptionManager,
  message: string,
  eventName: string
) => {
  const checkFields = message === 'update'
  const parts = eventName.split('.')
  const endField = parts.slice(1).join('.')
  const field = eventName
  const id = parts[0]

  if (endField !== 'type') {
    for (let key in subsManager.queries) {
      if (!subsManager.inProgress[key]) {
        if (subsManager.inProgress[key]) {
          continue
        }

        const q = subsManager.queries[key]

        let needsUpdate = false
        for (let i = 0; i < q.length; i++) {
          const item = q[i]
          const idFields = item.idFields

          if (idFields && idFields[field]) {
            needsUpdate = true
            break
          }

          const ids = item.ids

          if (ids) {
            if (!ids[id]) {
              continue
            }
          }

          const types = item.type

          if (types) {
            let isOfType = false
            for (let j = 0; j < types.length; j++) {
              if (id.slice(0, 2) === types[j]) {
                isOfType = true
                break
              }
            }
            if (!isOfType) {
              continue
            }
          }

          const fields = item.fields

          if (checkFields) {
            let notField = true
            const [startOfEndField] = endField.split('.')
            for (let field in fields) {
              if (field === endField || startOfEndField === field) {
                notField = false
                break
              }
            }
            if (notField) {
              continue
            }
          }

          if (!ids && !membersContainsId(subsManager, id, item, key)) {
            // waits a bit checks if alrdy in progress
            continue
          }

          needsUpdate = true
          break
        }

        if (needsUpdate) {
          addToUpdateQueue(subsManager, key)
        }
      }
    }
  }
}

export default handleQuery
