import { SubscriptionManager, Subscription } from './types'
import { constants } from '@saulx/selva'
import addUpdate from './update/addUpdate'
import { ServerSelector } from '@saulx/selva/dist/src/types'

const { EVENTS, SUBSCRIPTION_UPDATE } = constants

const prefixLength = SUBSCRIPTION_UPDATE.length
const deleteLength = 'delete:'.length

// pass subscription
const addOriginListeners = async (
  name: string,
  subsManager: SubscriptionManager,
  subscription: Subscription
) => {
  // we need to use name and unsubscribe as well
  const selector: ServerSelector = { name }
  const { client } = subsManager
  const serverDescriptor = await client.getServer(selector)
  subscription.originDescriptors[name] = serverDescriptor

  if (!subsManager.originListeners[name]) {
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
        const subId = channel.slice(prefixLength)
        const subscription = subsManager.subscriptions[subId]

        if (subscription) {
          addUpdate(subsManager, subscription)
        }

        // make this batch as well (the check)
        // if (message === 'update') {
        //   traverseTree(subsManager, eventName, name)
        // } else if (message && message.startsWith('delete')) {
        //   const fields = message.slice(deleteLength).split(',')

        //   fields.forEach((v: string) => {
        //     traverseTree(subsManager, eventName + '.' + v, name)
        //   })
        // }
      }

      if (!subsManager.stagedInProgess) {
        subsManager.incomingCount = 0
      }
    }

    subsManager.originListeners[name] = {
      subscriptions: new Set(),
      listener,
      reconnectListener: descriptor => {
        subscription.originDescriptors[name] = serverDescriptor
        const { name: dbName } = descriptor

        // TODO: replace descriptor
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

    const redis = client.redis

    // make this better
    // use this with the global connectorClients
    client.on('reconnect', subsManager.originListeners[name].reconnectListener)

    redis.on(serverDescriptor, 'pmessage', listener)
    redis.psubscribe(serverDescriptor, SUBSCRIPTION_UPDATE + '*')
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
      redis.punsubscribe({ name }, SUBSCRIPTION_UPDATE + '*')
      client.removeListener('reconnect', origin.reconnectListener)
      redis.removeListener({ name }, 'pmessage', origin.listener)
      delete subsManager.originListeners[name]
    }
  }
}

export { addOriginListeners, removeOriginListeners }
