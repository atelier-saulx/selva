import { GetOptions, GetResult } from '../get/types'
import { EventEmitter } from 'events'

import { getClient } from './clients'
import getServerDescriptor from './getServerDescriptor'
import RedisSelvaClient from './'
import Observable from '../observe/observable'

class Subscription extends EventEmitter {
  public count: number = 0
  constructor() {
    super()
    this.setMaxListeners(1e3)
  }
}

// createSubscription
// also needs to keep the server it is connected to in check with registry (has to change it)

const removeSubscription = (client: RedisSelvaClient, channel: string) => {
  console.log('remove it!')
  delete client.observables[channel]
  delete client.subscriptions[channel]
}

const createSubscription = (
  client: RedisSelvaClient,
  channel: string,
  opts: GetOptions
): Observable<GetResult> => {
  // first check if it exists

  if (client.observables[channel]) {
    // allrdy exists re-use!
    return client.observables[channel]
  }

  // does this need to be an event emitter or can we just send the command?
  // with one listener
  const subscription = new Subscription()

  const obs = new Observable(observer => {
    console.log('yes')
    subscription.count++
    return () => {
      subscription.count--
      if (subscription.count === 0) {
        removeSubscription(client, channel)
      }
    }
  })

  client.observables[channel] = obs
  client.subscriptions[channel] = subscription

  return obs
}

export { createSubscription, Subscription }

/*
// only add this once
 client.on({ type: 'registry' }, 'message', channel => {
    if (channel === REGISTRY_UPDATE) {
      // console.log('REGISTRY UPDATED (could be a new client!')
      getServers(client)
    }
  })
*/
