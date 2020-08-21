import { GetOptions, GetResult } from '../get/types'
import { EventEmitter } from 'events'
import {
  startObserver,
  stopObserver,
  getObserverValuePromised
} from './clients/observers'
import { getClient, Client } from './clients'
import getServerDescriptor from './getServerDescriptor'
import RedisSelvaClient from './'
import Observable from '../observe/observable'
import validate, { ExtraQueries } from '../get/validate'

class ObserverEmitter extends EventEmitter {
  public count: number = 0
  public isSend: boolean = false
  public getOptions: GetOptions
  public channel: string
  public client: Client
  public validationError?: Error
  public isRemoved: boolean = false
  constructor(getOptions: GetOptions, channel: string) {
    super()
    this.channel = channel
    this.getOptions = getOptions
    this.setMaxListeners(1e4)
  }
}

const attachClient = (
  redisSelvaClient: RedisSelvaClient,
  observerEmitter: ObserverEmitter,
  channel: string
) => {
  if (!redisSelvaClient.registry) {
    redisSelvaClient.selvaClient.once('connect', () => {
      attachClient(redisSelvaClient, observerEmitter, channel)
    })
  } else {
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
}

const createObservable = (
  redisSelvaClient: RedisSelvaClient,
  channel: string,
  opts: GetOptions
): Observable<GetResult> => {
  if (redisSelvaClient.observables[channel]) {
    return redisSelvaClient.observables[channel]
  }

  const observerEmitter = new ObserverEmitter(opts, channel)
  observerEmitter.on('error', () => {})

  const extraQueries: ExtraQueries = {}

  const obs = new Observable(observer => {
    const updateListener = obj => {
      if (obj.type === 'update') {
        if (obj.version !== observer.version) {
          observer.version = obj.version
          observer.next(obj.payload)
        }
      }
    }
    observerEmitter.on('update', updateListener)

    let errorListener
    if (observer.error) {
      errorListener = observer.error
      observerEmitter.on('error', errorListener)
      if (observerEmitter.validationError) {
        console.error(
          'Invalid query',
          opts,
          observerEmitter.validationError.message
        )

        observer.error(observerEmitter.validationError)
      }
    }

    if (observerEmitter.client) {
      // this will be much nicer with a cache
      getObserverValuePromised(observerEmitter.client, channel).then(obj => {
        if (obj) {
          updateListener(obj)
        }
      })
    }

    observerEmitter.count++
    return () => {
      if (errorListener) {
        observerEmitter.removeListener('error', errorListener)
      }
      observerEmitter.removeListener('update', updateListener)
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

  validate(extraQueries, redisSelvaClient.selvaClient, opts)
    .then(() => {
      attachClient(redisSelvaClient, observerEmitter, channel)
    })
    .catch(err => {
      observerEmitter.validationError = err
      console.log('is validated!')
      observerEmitter.emit('error', err)
      console.error('Invalid query', opts, err.message)
    })

  return obs
}

export const subsmanagerRemoved = (
  redisSelvaClient: RedisSelvaClient,
  id: string
) => {
  console.log('subs manager removed')
  for (const channel in redisSelvaClient.observerEmitters) {
    const observerEmitter = redisSelvaClient.observerEmitters[channel]
    if (observerEmitter.client.id === id) {
      console.log(
        'Need to re-apply this observer, subs manager is unregistered'
      )
      stopObserver(observerEmitter.client, channel, observerEmitter)
      attachClient(redisSelvaClient, observerEmitter, channel)
    }
  }
}

export { createObservable, ObserverEmitter }
