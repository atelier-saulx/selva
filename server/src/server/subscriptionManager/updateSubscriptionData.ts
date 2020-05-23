import { SubscriptionManager } from './types'
import { constants, GetOptions } from '@saulx/selva'
import { addSubscription } from './addSubscription'
import { removeSubscription } from './removeSubscription'

const { SUBSCRIPTIONS, CLIENTS } = constants

const updateSubscriptionData = async (subsManager: SubscriptionManager) => {
  const { selector, client } = subsManager
  const { redis } = client

  let [subscriptions, clients] = await Promise.all([
    redis.hgetall(selector, SUBSCRIPTIONS),
    redis.hgetall(selector, CLIENTS)
  ])

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
    if (now - ts < 60e3) {
      // client is not timed out
      if (client in subsManager.clients) {
        subsManager.clients[client].lastTs = ts
      } else {
        // no client add it
        subsManager.clients[client] = {
          subscriptions: new Set(),
          lastTs: ts
        }
      }
    } else {
      console.log('Client is timedout from server', client)
      cleanUpQ.push(redis.hdel(selector, CLIENTS, client))
      if (client in subsManager.clients) {
        // need to remove client
        delete subsManager.clients[client]
      }
    }
  }

  await Promise.all(
    Object.keys(subscriptions).map(async channel => {
      const subscriptionClients = await redis.smembers(selector, channel)
      if (channel in subsManager.subscriptions) {
        for (let i = subscriptionClients.length - 1; i >= 0; i--) {
          const client = subscriptionClients[i]
          if (!subsManager.clients[client]) {
            subscriptionClients.splice(i, 1)
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
          console.log('ADD FROM UPDATE')
          addSubscription(
            subsManager,
            channel,
            clientsSet,
            <GetOptions>JSON.parse(subscriptions[channel])
          )
        }
      }
    })
  )

  if (cleanUpQ.length) {
    await Promise.all(cleanUpQ)
    console.log('Cleaned up clients / subscriptions', cleanUpQ.length)
  }
}

export default updateSubscriptionData
