import SubscriptionManager from '../subsManager'
import { prefixes } from '@saulx/selva'
import traverseTree from './traverseTree'
import addUpdate from './addUpdate'

const prefixLength = prefixes.events.length

const addListeners = (subsManager: SubscriptionManager) => {
  // process.nextTick

  subsManager.client.redis.redis.sub.on(
    'pmessage',
    (_pattern, channel, message) => {
      subsManager.incomingCount++
      // use this for batching here
      // merge tree for checks?
      if (message === 'schema_update') {
        addUpdate(subsManager, '___selva_subscription:schema_update', true)
        return
      } else {
        const eventName = channel.slice(prefixLength)
        // make this batch as well (the check)
        if (message === 'update') {
          traverseTree(subsManager, eventName)
        } else if (message === 'delete') {
          traverseTree(subsManager, eventName, true)
        }
      }
    }
  )
  subsManager.client.redis.redis.sub.psubscribe(prefixes.events + '*')
}

export default addListeners
