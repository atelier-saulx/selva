import SubscriptionManager from './subsManager'
import { prefixes } from '@saulx/selva'

const addListeners = async (
  subsManager: SubscriptionManager
): Promise<void> => {
  console.log('Server add listeners')

  // this is on the subs manager
  subsManager.client.redis.redis.sSub.on('message', (channel, message) => {
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
        console.log('client heartbeat', client, ts)
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
      console.log('Got a create sub on (server)', channel)
      subsManager.addClientSubscription(client, channel)
    } else if (channel === prefixes.remove) {
      const { client, channel } = JSON.parse(message)
      console.log('Got a remove sub on (server)', channel)
      subsManager.removeClientSubscription(client, channel)
    }
  })

  subsManager.client.redis.redis.sSub.subscribe(prefixes.new)
  subsManager.client.redis.redis.sSub.subscribe(prefixes.heartbeat)
  subsManager.client.redis.redis.sSub.subscribe(prefixes.remove)
}

export default addListeners
