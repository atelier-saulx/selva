import SubscriptionManager from './index'
import { QuerySubscription } from '../../../lua/src/get/query/types'

const membersContainsId = async (
  subsManager: SubscriptionManager,
  id: string,
  item: QuerySubscription
): Promise<boolean> => {
  const member = item.member
  for (let j = 0; j < member.length; j++) {
    const m = member[j]
    const value = m.$value
    if (m.$field === 'ancestors') {
      // make this a lua script perhaps -- very heavy
      for (let k = 0; k < value.length; k++) {
        const v = value[k]
        if (v === 'root') {
          return true
        }
        // prob beyyer to just get ancetors
        // becomes async shitty - much better to do this loop in lua...
        const x = await subsManager.client.redis.command(
          'zscore',
          `${id}.ancestors`,
          v
        )
        if (x) {
          return true
        }
      }
    } else {
      for (let k = 0; k < value.length; k++) {
        const v = value[k]
        const x = await subsManager.client.redis.command(
          'smember',
          `${id}.${m.$field}`,
          v
        )
        if (x) {
          return true
        }
      }
    }
  }
  return false
}

const handleQuery = async (
  subsManager: SubscriptionManager,
  message: string,
  eventName: string
) => {
  // TYPE needs to be fixed

  const checkFields = message === 'update'

  const parts = eventName.split('.')
  const endField = parts.slice(1).join('.')
  const field = eventName
  const id = parts[0]
  if (endField !== 'type') {
    for (let key in subsManager.queries) {
      if (subsManager.inProgress[key]) {
        continue
      }

      const q = subsManager.queries[key]
      let needsUpdate = false
      for (let i = 0; i < q.length; i++) {
        const item = q[i]
        const idFields = item.idFields
        //   const type = item.

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
          for (let field in fields) {
            if (field === endField) {
              notField = false
              break
            }
          }
          if (notField) {
            continue
          }
        }

        if (!ids && !membersContainsId(subsManager, id, item)) {
          continue
        }

        needsUpdate = true
        break
      }

      if (needsUpdate) {
        subsManager.inProgress[key] = true
        subsManager.cleanUpProgress()
        setTimeout(() => {
          subsManager.sendUpdate(key).catch(e => {
            console.error('ERROR QUERY SUBS UPDATE', e)
          })
        }, 0)
      }
    }
  }
}

export default handleQuery
