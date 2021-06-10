import { SubscriptionManager, Subscription } from './types'
import { constants } from '@saulx/selva'
import addUpdate from './update/addUpdate'
import sendUpdate from './update/sendUpdate'
import { ServerSelector } from '@saulx/selva/dist/src/types'

const { EVENTS, SUBSCRIPTION_UPDATE, TRIGGER_UPDATE } = constants

const prefixLength = SUBSCRIPTION_UPDATE.length
const triggerPrefixLength = TRIGGER_UPDATE.length
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

    const listener = (pattern, channel, message) => {
      subsManager.incomingCount++
      collect++

      if (message === 'schema_update') {
        const subscription =
          subsManager.subscriptions[`${constants.SCHEMA_SUBSCRIPTION}:${name}`]
        if (subscription) {
          addUpdate(subsManager, subscription)
        }
      } else {
        console.log('GOT EVENT', channel, message)
        if (pattern.startsWith(SUBSCRIPTION_UPDATE)) {
          const subId = channel.slice(prefixLength)
          const subscription = subsManager.subscriptions[subId]

          if (subscription) {
            addUpdate(subsManager, subscription)
          }
        } else if (pattern.startsWith(TRIGGER_UPDATE)) {
          const subId = channel.slice(triggerPrefixLength)
          const subscription = subsManager.subscriptions[subId]
          const nodeId = message

          // TODO: maybe do some staging for updates stuff if nodeId is also the same?
          sendUpdate(subsManager, subscription, nodeId)
        }
      }

      if (!subsManager.stagedInProgess) {
        subsManager.incomingCount = 0
      }
    }

    subsManager.originListeners[name] = {
      subscriptions: new Set(),
      listener,
      reconnectListener: (descriptor) => {
        const { name: dbName } = descriptor

        // not enough ofcourse
        if (name === dbName) {
          console.info(
            'reconn in subs manager - need to only do reconn  when we are actively connected to this server...',
            name,
            descriptor
          )

          // need to resend subs if it dc'ed
          const origin = subsManager.originListeners[name]
          if (origin && origin.subscriptions) {
            origin.subscriptions.forEach((subscription) => {
              subscription.originDescriptors[name] = descriptor
              addUpdate(subsManager, subscription)
            })
          }
        }
      },
      serverUpdateListener: (payload) => {
        if (payload.event === 'new') {
          const { server } = payload
          const current = subscription.originDescriptors[name]
          if (server.name !== current.name) {
            return
          }

          if (current.type === 'origin' && server.type === 'replica') {
            console.info(
              'Upgrading connection from origin to replica',
              current,
              server
            )
            const subscriptions = new Set([
              ...subsManager.originListeners[name].subscriptions,
            ])
            for (const sub of subscriptions) {
              removeOriginListeners(name, subsManager, sub)
            }

            for (const sub of subscriptions) {
              sub.originDescriptors[name] = server
              addOriginListeners(name, subsManager, sub).finally(() =>
                addUpdate(subsManager, subscription)
              )
            }
          }
        }
      },
    }

    const redis = client.redis

    // make this better
    // use this with the global connectorClients
    client.on('reconnect', subsManager.originListeners[name].reconnectListener)
    client.on(
      'added-servers',
      subsManager.originListeners[name].serverUpdateListener
    )
    client.on(
      'removed-servers',
      subsManager.originListeners[name].serverUpdateListener
    )

    redis.on(serverDescriptor, 'pmessage', listener)
    redis.psubscribe(serverDescriptor, '___selva_events:*')
    redis.psubscribe(serverDescriptor, SUBSCRIPTION_UPDATE + '*')
    redis.psubscribe(serverDescriptor, TRIGGER_UPDATE + '*')
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
      redis.punsubscribe({ name }, SUBSCRIPTION_UPDATE + '*')
      redis.punsubscribe({ name }, TRIGGER_UPDATE + '*')
      redis.punsubscribe({ name }, '___selva_events:*')
      client.removeListener('reconnect', origin.reconnectListener)
      client.removeListener('added-servers', origin.serverUpdateListener)
      client.removeListener('removed-servers', origin.serverUpdateListener)
      redis.removeListener({ name }, 'pmessage', origin.listener)
      delete subsManager.originListeners[name]
    }
  }
}

export { addOriginListeners, removeOriginListeners }
