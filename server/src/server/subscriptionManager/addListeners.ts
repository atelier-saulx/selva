import { SubscriptionManager } from './types'
import { constants } from '@saulx/selva'
import { addClientSubscription } from './addSubscription'
import { removeClientSubscription } from './removeSubscription'

const {
  HEARTBEAT,
  CLIENTS,
  REMOVE_SUBSCRIPTION,
  NEW_SUBSCRIPTION,
  STOP_HEARTBEAT,
} = constants

const addListeners = async (
  subsManager: SubscriptionManager
): Promise<void> => {
  const { selector } = subsManager
  const redis = subsManager.client.redis
  redis.on(selector, 'message', (channel, message) => {
    if (channel === STOP_HEARTBEAT) {
      if (message in subsManager.clients) {
        subsManager.clients[message].subscriptions.forEach((channel) => {
          removeClientSubscription(subsManager, message, channel)
        })
        delete subsManager.clients[message]
      }
    } else if (channel === HEARTBEAT) {
      const { client, ts } = JSON.parse(message)
      if (!subsManager.clients[client]) {
        subsManager.clients[client] = { subscriptions: new Set(), lastTs: ts }
        redis.hset(selector, CLIENTS, client, ts)
      } else {
        subsManager.clients[client].lastTs = ts
        redis.hset(selector, CLIENTS, client, ts)
      }
    } else if (channel === NEW_SUBSCRIPTION) {
      const { client, channel } = JSON.parse(message)
      addClientSubscription(subsManager, client, channel)
    } else if (channel === REMOVE_SUBSCRIPTION) {
      const { client, channel } = JSON.parse(message)
      removeClientSubscription(subsManager, client, channel)
    }
  })

  redis.subscribe(selector, STOP_HEARTBEAT)
  redis.subscribe(selector, NEW_SUBSCRIPTION)
  redis.subscribe(selector, HEARTBEAT)
  redis.subscribe(selector, REMOVE_SUBSCRIPTION)
}

export default addListeners
