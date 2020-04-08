import SubscriptionManager from './subsManager'
import query from './query'
import { prefixes } from '@saulx/selva'

const addListeners = async (
  subsManager: SubscriptionManager
): Promise<void> => {
  console.log('Server add listeners')

  // this is on the subs manager
  subsManager.sSub.on('message', (channel, message) => {
    if (channel === prefixes.heartbeat) {
      const { client, ts } = JSON.parse(message)
      if (!subsManager.clients[client]) {
        if (client !== subsManager.client.redis.redis.uuid) {
          console.log('received new client on server', client)
          subsManager.clients[client] = { subscriptions: new Set(), lastTs: ts }
          subsManager.client.redis.byType.hset(
            'sClient',
            prefixes.clients,
            client,
            ts
          )
        }
      } else {
        subsManager.clients[client].lastTs = ts
        subsManager.client.redis.byType.hset(
          'sClient',
          prefixes.clients,
          client,
          ts
        )
      }
    } else if (channel === prefixes.new) {
      const { client, channel } = JSON.parse(message)
      console.log('Got a create sub on (server)', channel.slice(-5))
      subsManager.addClientSubscription(client, channel)
    } else if (channel === prefixes.remove) {
      const { client, channel } = JSON.parse(message)
      console.log('Got a remove sub on (server)', channel.slice(-5))
      subsManager.removeClientSubscription(client, channel)
    }
  })

  const prefixLength = prefixes.events.length

  // this is on the actual db
  subsManager.sub.on('pmessage', (_pattern, channel, message) => {
    subsManager.incomingCount++
    const updatedSubscriptions: Record<string, true> = {}
    const eventName = channel.slice(prefixLength)
    if (message === 'delete') {
      for (const field in subsManager.fieldMap) {
        if (field.startsWith(eventName)) {
          const subscriptionIds: Set<string> =
            subsManager.fieldMap[field] || new Set()
          for (const subscriptionId of subscriptionIds) {
            if (updatedSubscriptions[subscriptionId]) {
              continue
            }
            updatedSubscriptions[subscriptionId] = true
            subsManager.sendUpdate(subscriptionId, true)
          }
        }
      }
      return
    } else if (message === 'update') {
      const parts = eventName.split('.')
      let field = parts[0]
      for (let i = 0; i < parts.length; i++) {
        const channels: Set<string> | undefined = subsManager.fieldMap[field]
        if (channels) {
          for (const channel of channels) {
            if (updatedSubscriptions[channel]) {
              continue
            }
            updatedSubscriptions[channel] = true
            subsManager.sendUpdate(channel)
          }
        }
        if (i < parts.length - 1) {
          field += '.' + parts[i + 1]
        }
      }
    }
    // if query dont add to fields
    query(subsManager, message, eventName)
  })

  subsManager.sub.psubscribe(prefixes.events + '*')

  subsManager.sSub.subscribe(prefixes.new)
  subsManager.sSub.subscribe(prefixes.heartbeat)
  subsManager.sSub.subscribe(prefixes.remove)
}

export default addListeners
