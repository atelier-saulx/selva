import { GetOptions, GetResult } from '../get/types'
import { EventEmitter } from 'events'
import { startObserver, stopObserver } from './clients/observers'
import { getClient, Client } from './clients'
import getServerDescriptor from './getServerDescriptor'
import RedisSelvaClient from './'
import Observable from '../observe/observable'

class ObserverEmitter extends EventEmitter {
  public count: number = 0
  constructor() {
    super()
    this.setMaxListeners(1e3)
  }
}

// createSubscription
// also needs to keep the server it is connected to in check with registry (has to change it)

const createObservable = (
  redisSelvaClient: RedisSelvaClient,
  channel: string,
  opts: GetOptions
): Observable<GetResult> => {
  if (redisSelvaClient.observables[channel]) {
    return redisSelvaClient.observables[channel]
  }

  // does this need to be an event emitter or can we just send the command?
  // with one listener
  const observerEmitter = new ObserverEmitter()
  let isRemoved = false
  let client: Client

  getServerDescriptor(redisSelvaClient, {
    type: 'subscriptionManager'
    // channel
  }).then(descriptor => {
    if (!isRemoved) {
      client = getClient(redisSelvaClient, descriptor)
      startObserver(client, channel, opts, observerEmitter)
    }
  })

  const obs = new Observable(observer => {
    observerEmitter.count++
    return () => {
      observerEmitter.count--
      if (observerEmitter.count === 0) {
        isRemoved = true
        delete redisSelvaClient.observables[channel]
        delete redisSelvaClient.observerEmitters[channel]
        if (client) {
          stopObserver(client, channel, observerEmitter)
        }
      }
    }
  })

  observerEmitter.on('update', obj => {
    console.log('flapper drol', obj)
  })

  redisSelvaClient.observables[channel] = obs
  redisSelvaClient.observerEmitters[channel] = observerEmitter

  return obs
}

export { createObservable, ObserverEmitter }
