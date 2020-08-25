import { getClient } from './connection'
import getServerDescriptor from './getServerDescriptor'
import RedisSelvaClient from './'
import { Callback } from './types'
import handleListenerClient from './connection/handleListenerClient'

const handleListener = (
  redisSelvaClient: RedisSelvaClient,
  method: string,
  selector: any,
  event: any,
  callback?: Callback
) => {
  if (!redisSelvaClient.registry.connection) {
    redisSelvaClient.listenerQueue.push({ selector, event, callback })
  } else {
    if (typeof selector === 'string') {
      callback = event
      event = selector
      // if replica is available
      selector = { name: 'default', type: 'replica' }
    }
    if (selector.type === 'registry') {
      // what if it does not exist?
      redisSelvaClient.registry.connection.subscriber[method](event, callback)
      handleListenerClient(redisSelvaClient.registry, method, event, callback)
    } else {
      if (!selector.type && !selector.host) {
        selector.type = 'replica'
      }
      getServerDescriptor(redisSelvaClient.registry, selector).then(
        descriptor => {
          const client = getClient(redisSelvaClient, descriptor)
          handleListenerClient(client, method, event, callback)
        }
      )
    }
  }
}

export default handleListener
