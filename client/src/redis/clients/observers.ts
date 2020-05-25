import { GetOptions } from '../../get'
import { ObserverEmitter } from '../observers'
import { Client, addCommandToQueue } from './'
import {
  NEW_SUBSCRIPTION,
  SUBSCRIPTIONS,
  REMOVE_SUBSCRIPTION,
  CACHE
} from '../../constants'

// get them for all
export const getObserverValue = (
  client: Client,
  channel: string,
  observerEmitter?: ObserverEmitter
) => {
  addCommandToQueue(client, {
    command: 'hmget',
    args: [CACHE, channel, channel + '_version'],
    resolve: ([data, version]) => {
      if (data) {
        const obj = JSON.parse(data)
        obj.version = version
        if (observerEmitter) {
          observerEmitter.isSend = true
          observerEmitter.emit('update', obj)
        } else {
          if (client.observers[channel]) {
            client.observers[channel].forEach(observerEmitter => {
              observerEmitter.isSend = true
              observerEmitter.emit('update', obj)
            })
          }
        }
      } else {
        if (observerEmitter) {
          observerEmitter.isSend = true
        } else if (client.observers[channel]) {
          client.observers[channel].forEach(observerEmitter => {
            observerEmitter.isSend = true
          })
        }
      }
    },
    reject: err => {
      // @ts-ignore
      if (err.code !== 'UNCERTAIN_STATE') {
        console.error(err.message)
      }
    }
  })
}

export function sendObserver(
  client: Client,
  channel: string,
  getOptions: GetOptions
) {
  addCommandToQueue(client, {
    command: 'hsetnx',
    args: [SUBSCRIPTIONS, channel, JSON.stringify(getOptions)]
  })
  addCommandToQueue(client, {
    command: 'sadd',
    args: [channel, client.uuid]
  })
  addCommandToQueue(client, {
    command: 'publish',
    args: [NEW_SUBSCRIPTION, JSON.stringify({ client: client.uuid, channel })]
  })
  client.subscriber.subscribe(channel)
}

export function startObserver(
  client: Client,
  channel: string,
  observerEmitter: ObserverEmitter
) {
  if (!client.observers[channel]) {
    client.observers[channel] = new Set()
    sendObserver(client, channel, observerEmitter.getOptions)
  }
  client.observers[channel].add(observerEmitter)
  getObserverValue(client, channel, observerEmitter)
}

export function stopObserver(
  client: Client,
  channel: string,
  observerEmitter: ObserverEmitter
) {
  if (client.observers[channel]) {
    client.observers[channel].delete(observerEmitter)
    if (client.observers[channel].size === 0) {
      addCommandToQueue(client, {
        command: 'srem',
        args: [channel, client.uuid]
      })
      addCommandToQueue(client, {
        command: 'publish',
        args: [
          REMOVE_SUBSCRIPTION,
          JSON.stringify({ client: this.uuid, channel })
        ]
        // resolve: () => this.removeSubscriptionsSet.delete(channel)
      })
      client.subscriber.unsubscribe(channel)
      delete client.observers[channel]
    }
  }
}
