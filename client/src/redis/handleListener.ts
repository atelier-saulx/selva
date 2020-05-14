import { getClient } from './clients'
import getServerDescriptor from './getServerDescriptor'
import RedisSelvaClient from './'
import { Callback } from './types'

const handleListener = (
  redisSelvaClient: RedisSelvaClient,
  method: string,
  selector: any,
  event: any,
  callback?: any
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
    } else {
      if (!selector.type && !selector.host) {
        selector.type = 'replica'
      }
      getServerDescriptor(redisSelvaClient, selector).then(descriptor => {
        const client = getClient(redisSelvaClient, descriptor)
        client.subscriber[method](event, callback)
      })
    }
  }
}

export default handleListener
