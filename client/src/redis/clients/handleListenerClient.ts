import { Client } from '.'
import { Callback } from '../types'

const handleListenerClient = (
  client: Client,
  method: string,
  event: string,
  callback?: Callback
) => {
  if (method === 'on') {
    client.subscriber.on(event, callback)
    if (!client.redisListeners[event]) {
      client.redisListeners[event] = []
    }
    client.redisListeners[event].push(callback)
  } else if (method === 'removeListener') {
    if (callback) {
      client.subscriber.removeListener(event, callback)
      if (client.redisListeners[event]) {
        const index = client.redisListeners[event].indexOf(callback)
        if (index !== -1) {
          client.redisListeners[event].splice(index, 1)
        }
        if (client.redisListeners[event].length === 0) {
          delete client.redisListeners[event]
        }
      }
    }
  } else if (method === 'removeAllListeners') {
    if (event) {
      client.subscriber.removeAllListeners(event)
      delete client.redisListeners[event]
    } else {
      client.redisListeners = {}
      client.subscriber.removeAllListeners()
    }
  }
}

export default handleListenerClient
