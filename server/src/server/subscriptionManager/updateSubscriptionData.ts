import { SubscriptionManager } from './types'
import { constants, GetOptions } from '@saulx/selva'
import { addSubscription } from './addSubscription'
import { removeSubscription } from './removeSubscription'
import updateRegistry from './updateRegistrySubscriptions'

const { SUBSCRIPTIONS, CLIENTS } = constants

const updateSubscriptionData = async (subsManager: SubscriptionManager) => {
  const { selector, client } = subsManager
  const { redis } = client

  //  [channel]: 'created'
  const info = {
    ...subsManager.selector,
    subscriptions: {},
  }

  // if it just restarted clients did not resubscribe themselves

  let [subscriptions, clients] = await Promise.all([
    redis.hgetall(selector, SUBSCRIPTIONS),
    redis.hgetall(selector, CLIENTS),
  ])

  if (!clients && subscriptions) {
    console.info('NO CLIENT BUT SUBS')
  }

  if (!subscriptions) {
    subscriptions = {}
  }

  if (!clients) {
    clients = {}
  }

  const now = Date.now()
  const cleanUpQ = []

  for (const client in clients) {
    const ts = Number(clients[client])
    if (now - ts < 10e3) {
      // client is not timed out
      if (client in subsManager.clients) {
        subsManager.clients[client].lastTs = ts
      } else {
        // no client add it
        subsManager.clients[client] = {
          subscriptions: new Set(),
          lastTs: ts,
        }
      }
    } else {
      // this should get removed...
      console.info('Client is timedout from server', client)
      cleanUpQ.push(redis.hdel(selector, CLIENTS, client))
      if (client in subsManager.clients) {
        // need to remove client
        delete subsManager.clients[client]
      }
    }
  }

  await Promise.all(
    Object.keys(subscriptions).map(async (channel) => {
      const subscriptionClients = await redis.smembers(selector, channel)
      if (channel in subsManager.subscriptions) {
        for (let i = subscriptionClients.length - 1; i >= 0; i--) {
          const client = subscriptionClients[i]
          if (!subsManager.clients[client]) {
            subscriptionClients.splice(i, 1)
            subsManager.subscriptions[channel].clients.delete(client)
            cleanUpQ.push(redis.srem(selector, channel, client))
          }
        }

        if (subsManager.subscriptions[channel].clients.size === 0) {
          removeSubscription(subsManager, channel, cleanUpQ)
        }
      } else {
        const clientsSet: Set<string> = new Set()
        for (let i = subscriptionClients.length - 1; i >= 0; i--) {
          const client = subscriptionClients[i]
          if (client in subsManager.clients) {
            clientsSet.add(client)
          } else {
            subscriptionClients.splice(i, 1)
            cleanUpQ.push(redis.srem(selector, channel, client))
          }
        }
        if (clientsSet.size) {
          addSubscription(
            subsManager,
            channel,
            clientsSet,
            <GetOptions>JSON.parse(subscriptions[channel])
          )
        } else {
          info.subscriptions[channel] = 'removed'
          cleanUpQ.push(redis.hdel(selector, SUBSCRIPTIONS, channel))
        }
      }
    })
  )

  if (cleanUpQ.length) {
    await Promise.all(cleanUpQ)
    console.info('Cleaned up clients / subscriptions', cleanUpQ.length)
  }

  if (Object.keys(info.subscriptions).length) {
    updateRegistry(client, info, subsManager)
  }
}

export default updateSubscriptionData
