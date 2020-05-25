import { GetOptions, GetResult } from '../get/types'
import { EventEmitter } from 'events'
import { startObserver, stopObserver } from './clients/observers'
import { getClient, Client } from './clients'
import getServerDescriptor from './getServerDescriptor'
import RedisSelvaClient from './'
import Observable from '../observe/observable'

class ObserverEmitter extends EventEmitter {
  public count: number = 0
  public isSend: boolean = false
  public getOptions: GetOptions
  constructor(getOptions) {
    super()
    this.getOptions = getOptions
    this.setMaxListeners(1e4)
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
  const observerEmitter = new ObserverEmitter(opts)
  let isRemoved = false
  let client: Client

  getServerDescriptor(redisSelvaClient, {
    type: 'subscriptionManager',
    subscription: channel
  }).then(descriptor => {
    if (!isRemoved) {
      client = getClient(redisSelvaClient, descriptor)
      startObserver(client, channel, observerEmitter)
    }
  })

  const obs = new Observable(observer => {
    observerEmitter.on('update', obj => {
      if (obj.type === 'update') {
        if (obj.version !== observer.version) {
          observer.version = obj.version
          observer.next(obj.payload)
        }
      }
    })

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

  redisSelvaClient.observables[channel] = obs
  redisSelvaClient.observerEmitters[channel] = observerEmitter

  return obs
}

export { createObservable, ObserverEmitter }
