import SubscriptionManager from '../subsManager'
import { prefixes } from '@saulx/selva'
import traverseTree from './traverseTree'
import addUpdate from './addUpdate'

const prefixLength = prefixes.events.length
const deleteLength = 'delete:'.length

const addListeners = (subsManager: SubscriptionManager) => {
  // process.nextTick

  subsManager.client.redis.redis.sub.on(
    'pmessage',
    (_pattern, channel, message) => {
      subsManager.incomingCount++
      // use this for batching here
      // merge tree for checks?
      if (message === 'schema_update') {
        addUpdate(
          subsManager,
          subsManager.subscriptions['___selva_subscription:schema_update'],
          { type: message }
        )
        return
      } else {
        const eventName = channel.slice(prefixLength)
        // make this batch as well (the check)
        if (message === 'update') {
          traverseTree(subsManager, eventName)
        } else if (message && message.startsWith('delete')) {
          const fields = message.slice(deleteLength).split(',')
          fields.forEach(v => {
            traverseTree(subsManager, eventName + '.' + v)
          })
        }
      }
    }
  )
  subsManager.client.redis.redis.sub.psubscribe(prefixes.events + '*')
}

export default addListeners
