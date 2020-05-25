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
  public channel: string
  public client: Client
  public isRemoved: boolean = false
  constructor(getOptions: GetOptions, channel: string) {
    super()
    this.channel = channel
    this.getOptions = getOptions
    this.setMaxListeners(1e4)
  }
}

// createSubscription
// also needs to keep the server it is connected to in check with registry (has to change it)

const attachClient = (
  redisSelvaClient: RedisSelvaClient,
  observerEmitter: ObserverEmitter,
  channel: string
) => {
  getServerDescriptor(redisSelvaClient, {
    type: 'subscriptionManager',
    subscription: channel
  }).then(descriptor => {
    if (!observerEmitter.isRemoved) {
      observerEmitter.client = getClient(redisSelvaClient, descriptor)
      startObserver(observerEmitter.client, channel, observerEmitter)
    }
  })
}

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
  const observerEmitter = new ObserverEmitter(opts, channel)

  attachClient(redisSelvaClient, observerEmitter, channel)

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
        observerEmitter.isRemoved = true
        delete redisSelvaClient.observables[channel]
        delete redisSelvaClient.observerEmitters[channel]
        if (observerEmitter.client) {
          stopObserver(observerEmitter.client, channel, observerEmitter)
        }
      }
    }
  })

  redisSelvaClient.observables[channel] = obs
  redisSelvaClient.observerEmitters[channel] = observerEmitter

  return obs
}

export const subsmanagerRemoved = (
  redisSelvaClient: RedisSelvaClient,
  id: string
) => {
  // go
  for (const channel in redisSelvaClient.observerEmitters) {
    const observerEmitter = redisSelvaClient.observerEmitters[channel]
    if (observerEmitter.client.id === id) {
      console.log('need to re-apply this observer')
      stopObserver(observerEmitter.client, channel, observerEmitter)
      attachClient(redisSelvaClient, observerEmitter, channel)
    }
  }
}

export { createObservable, ObserverEmitter }
