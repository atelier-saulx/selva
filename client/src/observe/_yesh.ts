import { GetOptions, GetResult } from '../get/types'
import { EventEmitter } from 'events'

import { getConnection, Connection } from './connection'
import getServerDescriptor from './getServerDescriptor'
import Observable from '../observe/observable'
import validate, { ExtraQueries } from '../get/validate'
import { Registry } from './registry'

import SubscriptionEmitter from './emitter'

const attachClient = (
  registry: Registry,
  observerEmitter: ObserverEmitter,
  channel: string
) => {
  getServerDescriptor(registry, {
    type: 'subscriptionManager',
    subscription: channel
  }).then(descriptor => {
    if (!observerEmitter.isRemoved) {
      observerEmitter.client = getConnection(descriptor, registry)
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

  // make these on the actual connections

  // maybe a type subscription manager
  redisSelvaClient.observables[channel] = obs
  redisSelvaClient.observerEmitters[channel] = observerEmitter

  // ugly selvaClient that needs to be added....
  // better to add that on the schema
  // validate(extraQueries, redisSelvaClient.selvaClient, opts)
  //   .then(() => {
  //     attachClient(redisSelvaClient.registry, observerEmitter, channel)
  //   })
  //   .catch(err => {
  //     observerEmitter.validationError = err
  //     observerEmitter.emit('error', err)
  //     console.error('Invalid query', opts, err.message)
  //   })

  return obs
}

// do we need the selvaClient here? would be better wihout
export const subsmanagerRemoved = (
  redisSelvaClient: RedisSelvaClient,
  id: string
) => {
  for (const channel in redisSelvaClient.observerEmitters) {
    const observerEmitter = redisSelvaClient.observerEmitters[channel]
    if (observerEmitter.client.id === id) {
      stopObserver(observerEmitter.client, channel, observerEmitter)
      attachClient(redisSelvaClient.registry, observerEmitter, channel)
    }
  }
}

// does not need selva clients at all better to handle this somewhere else

export { createObservable, ObserverEmitter }
