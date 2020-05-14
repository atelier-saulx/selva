import { SubscriptionManager, Subscription } from './types'
import { constants, GetOptions } from '@saulx/selva'
import { hash } from './util'
import addUpdate from './update/addUpdate'
import { addSubscriptionToTree } from './tree'
import { addOriginListeners } from './originListeners'
import updateRegistry from './updateRegistrySubscriptions'

const { CACHE, SUBSCRIPTIONS } = constants

const addClientSubscription = async (
  subsManager: SubscriptionManager,
  client: string,
  channel: string
) => {
  const { selector } = subsManager
  const redis = subsManager.client.redis
  if (!subsManager.subscriptions[channel]) {
    const [getOptions, clients] = await Promise.all([
      redis.hget(selector, SUBSCRIPTIONS, channel),
      redis.smembers(selector, channel)
    ])
    if (getOptions && clients.length) {
      await addSubscription(
        subsManager,
        channel,
        new Set(clients),
        JSON.parse(getOptions)
      )
    }
  } else {
    subsManager.subscriptions[channel].clients.add(client)
  }
}

const parseOrigins = (
  getOptions: GetOptions,
  origins: Set<string> = new Set()
): Set<string> => {
  for (let key in getOptions) {
    if (key === '$db') {
      origins.add(getOptions[key])
    } else if (typeof getOptions[key] === 'object') {
      parseOrigins(getOptions[key], origins)
    }
  }
  return origins
}

const addSubscription = async (
  subsManager: SubscriptionManager,
  channel: string,
  clients: Set<string>,
  getOptions: GetOptions
) => {
  const { selector, client } = subsManager
  const { redis } = client
  const subscription: Subscription = {
    clients,
    channel,
    get: getOptions,
    origins: [...parseOrigins(getOptions).values()]
  }

  for (const origin of subscription.origins) {
    addOriginListeners(origin, subsManager, subscription)
  }

  updateRegistry(subsManager.client, {
    ...subsManager.selector,
    subscriptions: { [channel]: 'created' }
  })

  subsManager.subscriptions[channel] = subscription
  if (await redis.hexists(selector, CACHE, channel)) {
    const [tree, version] = await redis.hmget(
      selector,
      CACHE,
      channel + '_tree',
      channel + '_version'
    )

    if (!tree) {
      addUpdate(subsManager, subscription)
    } else {
      subsManager.subscriptions[channel].version = version
      subsManager.subscriptions[channel].tree = JSON.parse(tree)
      subsManager.subscriptions[channel].treeVersion = hash(tree)
      addSubscriptionToTree(subsManager, subscription)
    }
  } else {
    addUpdate(subsManager, subscription)
  }
}

export { addSubscription, addClientSubscription }
