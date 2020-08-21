import { GetOptions } from '../../get'
import { ObserverEmitter } from '../observers'
import { Client, addCommandToQueue } from './'
import {
  NEW_SUBSCRIPTION,
  SUBSCRIPTIONS,
  REMOVE_SUBSCRIPTION,
  CACHE
} from '../../constants'

export const getObserverValuePromised = (client, channel) =>
  new Promise((resolve, reject) => {
    addCommandToQueue(client, {
      command: 'hmget',
      args: [CACHE, channel, channel + '_version'],
      resolve: ([data, version]) => {
        if (data) {
          const obj = JSON.parse(data)
          obj.version = version
          resolve(obj)
        } else {
          resolve()
        }
      },
      reject
    })
  })

// get them for all
export const getObserverValue = (
  client: Client,
  channel: string,
  observerEmitter?: ObserverEmitter
) => {
  // this can better be handled on the observer function
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
          // extra else , cb
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

      // handle certain errors to the subscription
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
  addCommandToQueue(client, {
    command: 'subscribe',
    args: [channel]
  })
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
      })

      addCommandToQueue(client, { command: 'unsubscribe', args: [channel] })

      delete client.observers[channel]
    }
  }
}
