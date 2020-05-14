import { SubscriptionManager } from './types'
import { constants } from '@saulx/selva'
import { removeSubscriptionFromTree } from './tree'
import { removeOriginListeners } from './originListeners'
import updateRegistry from './updateRegistrySubscriptions'

const { CACHE, SUBSCRIPTIONS } = constants

const removeClientSubscription = async (
  subsManager: SubscriptionManager,
  client: string,
  channel: string
) => {
  const { selector } = subsManager
  const redis = subsManager.client.redis
  const clients = await redis.smembers(selector, channel)
  const sub = subsManager.subscriptions[channel]
  const cleanUpQ = []
  let len = clients.length
  if (clients.indexOf(client) !== -1) {
    len--
    cleanUpQ.push(redis.srem(selector, channel, client))
  }
  if (sub) {
    sub.clients.delete(client)
  }
  if (len === 0) {
    removeSubscription(subsManager, channel, cleanUpQ)
    delete subsManager.subscriptions[channel]
  }
  if (cleanUpQ.length) {
    await Promise.all(cleanUpQ)
  }
}

const removeSubscription = async (
  subsManager: SubscriptionManager,
  channel: string,
  cleanUpQ: any[] = []
) => {
  const { selector, client, subscriptions } = subsManager
  const { redis } = client

  updateRegistry(subsManager.client, {
    ...subsManager.selector,
    subscriptions: { [channel]: 'removed' }
  })

  cleanUpQ.push(redis.hdel(selector, SUBSCRIPTIONS, channel))
  cleanUpQ.push(redis.del(selector, channel))
  cleanUpQ.push(
    redis.hdel(
      selector,
      CACHE,
      channel,
      channel + '_version',
      channel + '_tree'
    )
  )
  if (channel in subscriptions) {
    const subscription = subscriptions[channel]
    for (const origin of subscription.origins) {
      removeOriginListeners(origin, subsManager, subscription)
    }
    removeSubscriptionFromTree(subsManager, subscription)
    delete subscriptions[channel]
  }
}

export { removeSubscription, removeClientSubscription }
