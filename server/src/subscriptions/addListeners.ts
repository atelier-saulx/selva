import SubscriptionManager from './index'
import query from './query'

const addListeners = async (
  subsManager: SubscriptionManager
): Promise<void> => {
  console.log('Server add listeners')

  const heartbeatChannel = '___selva_subscription:heartbeat'
  const newSubscriptionChannel = '___selva_subscription:new'
  const removeSubscriptionChannel = '___selva_subscription:remove'
  const clients = `___selva_clients`

  // this is on the subs manager
  subsManager.sub.on('message', (channel, message) => {
    if (channel === heartbeatChannel) {
      const { client, ts } = JSON.parse(message)
      if (!subsManager.clients[client]) {
        if (client !== subsManager.client.redis.redis.uuid) {
          console.log('received new client on server', client)
          subsManager.clients[client] = { subscriptions: new Set(), lastTs: ts }
          subsManager.client.redis.hset(clients, client, ts)
        }
      } else {
        subsManager.clients[client].lastTs = ts
        subsManager.client.redis.hset(clients, client, ts)
      }
    } else if (channel === newSubscriptionChannel) {
      const { client, channel } = JSON.parse(message)
      console.log('Got a create sub on (server)', channel.slice(-5))
      subsManager.addClientSubscription(client, channel)
    } else if (channel === removeSubscriptionChannel) {
      const { client, channel } = JSON.parse(message)
      console.log('Got a remove sub on (server)', channel.slice(-5))
      subsManager.removeClientSubscription(client, channel)
    }
  })

  const prefixLength = '___selva_events:'.length

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

  subsManager.sub.psubscribe('___selva_events:*')
  subsManager.sub.subscribe(newSubscriptionChannel)
  subsManager.sub.subscribe(heartbeatChannel)
  subsManager.sub.subscribe(removeSubscriptionChannel)
}

export default addListeners
