import { SubscriptionManager } from './types'
import { constants } from '@saulx/selva'
import { addClientSubscription } from './addSubscription'
import { removeClientSubscription } from './removeSubscription'

const { HEARTBEAT, CLIENTS, REMOVE_SUBSCRIPTION, NEW_SUBSCRIPTION } = constants

const addListeners = async (
  subsManager: SubscriptionManager
): Promise<void> => {
  console.log('Server add listeners')
  const { selector } = subsManager
  const redis = subsManager.client.redis

  redis.on(selector, 'message', (channel, message) => {
    if (channel === HEARTBEAT) {
      const { client, ts } = JSON.parse(message)
      if (!subsManager.clients[client]) {
        console.log('Received new client on server', client)
        subsManager.clients[client] = { subscriptions: new Set(), lastTs: ts }
        redis.hset(selector, CLIENTS, client, ts)
      } else {
        subsManager.clients[client].lastTs = ts
        redis.hset(selector, CLIENTS, client, ts)
      }
    } else if (channel === NEW_SUBSCRIPTION) {
      const { client, channel } = JSON.parse(message)
      // console.log('Got a create sub on (server)', channel)
      addClientSubscription(subsManager, client, channel)
    } else if (channel === REMOVE_SUBSCRIPTION) {
      const { client, channel } = JSON.parse(message)
      // console.log('Got a remove sub on (server)', channel)
      removeClientSubscription(subsManager, client, channel)
    }
  })

  redis.subscribe(selector, NEW_SUBSCRIPTION)
  redis.subscribe(selector, HEARTBEAT)
  redis.subscribe(selector, REMOVE_SUBSCRIPTION)
}

export default addListeners
