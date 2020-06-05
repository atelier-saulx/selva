import { getClient } from './clients'
import getServerDescriptor from './getServerDescriptor'
import RedisSelvaClient from './'
import { Callback } from './types'
import handleListenerClient from './clients/handleListenerClient'

const handleListener = (
  redisSelvaClient: RedisSelvaClient,
  method: string,
  selector: any,
  event: any,
  callback?: Callback
) => {
  if (!redisSelvaClient.registry) {
    redisSelvaClient.listenerQueue.push({ selector, event, callback })
  } else {
    if (typeof selector === 'string') {
      callback = event
      event = selector
      // if replica is available
      selector = { name: 'default', type: 'replica' }
    }
    if (selector.type === 'registry') {
      redisSelvaClient.registry.subscriber[method](event, callback)
      handleListenerClient(redisSelvaClient.registry, method, event, callback)
    } else {
      if (!selector.type && !selector.host) {
        selector.type = 'replica'
      }
      getServerDescriptor(redisSelvaClient, selector).then(descriptor => {
        const client = getClient(redisSelvaClient, descriptor)
        handleListenerClient(client, method, event, callback)
      })
    }
  }
}

export default handleListener
