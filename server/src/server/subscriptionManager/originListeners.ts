import { SubscriptionManager, Subscription } from './types'
import { constants } from '@saulx/selva'
import traverseTree from './update/traverseTree'
import addUpdate from './update/addUpdate'
import { ServerSelector } from '@saulx/selva/dist/src/types'

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
    const selector: ServerSelector = { name, type: 'replica' }

    console.log('ADD ORIGIN LISTENERS', name)

    const listener = (_pattern, channel, message) => {
      console.info('------------', name, channel, message)

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
      listener,
      reconnectListener: ({ name: dbName }) => {
        if (name === dbName) {
          console.log('RE-RUN ALL SUBSCRIPTIONS')
          const origin = subsManager.originListeners[name]
          if (origin && origin.subscriptions) {
            origin.subscriptions.forEach(subscription => {
              console.log('  ---> re fire sub', subscription.channel)
              addUpdate(subsManager, subscription)
            })
          }
        }
      }
    }

    const { client } = subsManager
    const redis = client.redis
    let collect = 0

    client.on('reconnect', subsManager.originListeners[name].reconnectListener)

    redis.on(selector, 'pmessage', listener)
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
      console.log('REMOVE ORIGIN LISTENERS', name)

      if (name in subsManager.memberMemCache) {
        delete subsManager.memberMemCache[name]
      }
      redis.punsubscribe({ name }, EVENTS + '*')
      client.removeListener('reconnect', origin.reconnectListener)
      redis.removeListener({ name }, 'pmessage', origin.listener)
      delete subsManager.originListeners[name]
    }
  }
}

export { addOriginListeners, removeOriginListeners }
