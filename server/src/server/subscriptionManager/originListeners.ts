import { SubscriptionManager, Subscription } from './types'
import { constants } from '@saulx/selva'
import traverseTree from './update/traverseTree'
import addUpdate from './update/addUpdate'
import { ServerSelector } from '@saulx/selva/dist/src/types'

const { EVENTS } = constants

const prefixLength = EVENTS.length
const deleteLength = 'delete:'.length

// pass subscription
const addOriginListeners = async (
  name: string,
  subsManager: SubscriptionManager,
  subscription: Subscription
) => {
  // we need to use name and unsubscribe as well

  if (!subsManager.originListeners[name]) {
    const selector: ServerSelector = { name }

    let collect = 0

    const listener = (_pattern, channel, message) => {
      subsManager.incomingCount++
      collect++

      if (message === 'schema_update') {
        const subscription =
          subsManager.subscriptions[`${constants.SCHEMA_SUBSCRIPTION}:${name}`]
        if (subscription) {
          addUpdate(subsManager, subscription)
        }
      } else {
        const eventName = channel.slice(prefixLength)

        console.log('INCOMING EVENT', eventName, name)

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
      reconnectListener: descriptor => {
        const { name: dbName } = descriptor
        console.log(
          'reconn in subs manager - need to only do reconn  when we are actively connected to this server...',
          name
        )

        // not enough ofcourse
        if (name === dbName) {
          // need to resend subs if it dc'ed
          const origin = subsManager.originListeners[name]
          if (origin && origin.subscriptions) {
            origin.subscriptions.forEach(subscription => {
              addUpdate(subsManager, subscription)
            })
          }
        }
      }
    }

    const { client } = subsManager
    const redis = client.redis

    // make this better
    // use this with the global connectorClients
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
      if (name in subsManager.memberMemCache) {
        delete subsManager.memberMemCache[name]
      }
      console.log('DELETE ORIGIN LISTENER')
      redis.punsubscribe({ name }, EVENTS + '*')
      client.removeListener('reconnect', origin.reconnectListener)
      redis.removeListener({ name }, 'pmessage', origin.listener)
      delete subsManager.originListeners[name]
    }
  }
}

export { addOriginListeners, removeOriginListeners }
