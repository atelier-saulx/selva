import SubscriptionManager from '../subsManager'
import { prefixes } from '@saulx/selva'
import traverseTree from './traverseTree'
import addUpdate from './addUpdate'

const prefixLength = prefixes.events.length
const deleteLength = 'delete:'.length

const addListeners = (subsManager: SubscriptionManager) => {
  // process.nextTick

  // setInterval(() => {
  //   console.log('handled ', subsManager.incomingCount, 'in last 10 sec')
  //   subsManager.incomingCount = 0
  // }, 10e3)

  subsManager.client.redis.redis.sub.on(
    'pmessage',
    (_pattern, channel, message) => {
      subsManager.incomingCount++
      // use this for batching here
      // merge tree for checks?
      if (message === 'schema_update') {
        const subscription =
          subsManager.subscriptions['___selva_subscription:schema_update']
        if (subscription) {
          addUpdate(subsManager, subscription)
        }
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

      if (!subsManager.stagedInProgess) {
        subsManager.incomingCount = 0
      }
    }
  )
  subsManager.client.redis.redis.sub.psubscribe(prefixes.events + '*')
}

export default addListeners
