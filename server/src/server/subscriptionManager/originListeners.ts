import { SubscriptionManager, Subscription } from './types'
import { constants } from '@saulx/selva'
import traverseTree from './update/traverseTree'
import addUpdate from './update/addUpdate'

const { EVENTS } = constants

const prefixLength = EVENTS.length
const deleteLength = 'delete:'.length

// pass subscription
const addOriginListeners = (
  name: string,
  subsManager: SubscriptionManager,
  subscription: Subscription
) => {
  // we need to use name and unsubscribe as well

  if (!subsManager.originListeners[name]) {
    const selector = { name }

    console.log('ADD DAT ORIGIN COME ON!', name)

    const listener = (_pattern, channel, message) => {
      subsManager.incomingCount++
      collect++
      // use this for batching here
      // merge tree for checks?
      if (message === 'schema_update') {
        const subscription =
          subsManager.subscriptions[`${constants.SCHEMA_SUBSCRIPTION}:${name}`]
        if (subscription) {
          addUpdate(subsManager, subscription)
        }
      } else {
        const eventName = channel.slice(prefixLength)
        // make this batch as well (the check)
        if (message === 'update') {
          traverseTree(subsManager, eventName, name)
        } else if (message && message.startsWith('delete')) {
          const fields = message.slice(deleteLength).split(',')
          fields.forEach((v: string) => {
            traverseTree(subsManager, eventName + '.' + v, name)
          })
        }
      }

      if (!subsManager.stagedInProgess) {
        subsManager.incomingCount = 0
      }
    }

    subsManager.originListeners[name] = {
      subscriptions: new Set(),
      listener
    }

    const { client } = subsManager
    const redis = client.redis
    let collect = 0
    setInterval(() => {
      console.log('handled ', collect, 'in last 5 sec')
      collect = 0
    }, 5e3)

    // check every origin - you have to connect to them :D
    redis.on(selector, 'pmessage', listener)

    // same EVERY SINGLE ONE - means you need a listener here on the registry
    redis.psubscribe(selector, EVENTS + '*')
  }

  subsManager.originListeners[name].subscriptions.add(subscription)
}

const removeOriginListeners = (
  name: string,
  subsManager: SubscriptionManager,
  subscription: Subscription
) => {
  const origin = subsManager.originListeners[name]
  if (origin) {
    const { client } = subsManager
    const redis = client.redis
    origin.subscriptions.delete(subscription)
    if (origin.subscriptions.size === 0) {
      redis.punsubscribe({ name }, EVENTS + '*')
      redis.removeListener({ name }, 'pmessage', origin.listener)
      delete subsManager.originListeners[name]
    }
  }
}

export { addOriginListeners, removeOriginListeners }
