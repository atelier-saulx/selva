import { GetOptions } from '../get'

import SubscriptionEmitter from '../observe/emitter'

import { Connection } from '.'
import {
  NEW_SUBSCRIPTION,
  SUBSCRIPTIONS,
  REMOVE_SUBSCRIPTION,
  CACHE
} from '../constants'

// too many thing here

const parseError = (obj: { [key: string]: any }) => {
  const err = obj.payload && obj.payload.___$error___
  if (typeof err === 'string') {
    return new Error(err)
  } else if (err.message) {
    return new Error(err.message)
  } else if (err.command) {
    const { command, args, code } = err
    if (command === 'EVALSHA') {
      return new Error(`Lua error ${args.slice(3).join(', ')}`)
    }
    return new Error(`${command} ${args.join(', ')} ${code}`)
  }
  return new Error('Unkown error')
}

export const getObserverValuePromised = (
  connection: Connection,
  channel: string
) =>
  new Promise((resolve, reject) => {
    connection.command({
      command: 'hmget',
      args: [CACHE, channel, channel + '_version'],
      resolve: ([data, version]) => {
        if (data) {
          const obj = JSON.parse(data)
          obj.version = version
          if (obj.payload && obj.payload.___$error___) {
            reject(parseError(obj))
          } else {
            resolve(obj)
          }
        } else {
          resolve()
        }
      },
      reject
    })
  })

// get them for all
export const getObserverValue = (
  connection: Connection,
  channel: string,
  subscriptionEmitter?: SubscriptionEmitter
) => {
  // this can better be handled on the observer function
  connection.command({
    command: 'hmget',
    args: [CACHE, channel, channel + '_version'],
    resolve: ([data, version]) => {
      if (data) {
        const obj = JSON.parse(data)
        obj.version = version

        if (subscriptionEmitter) {
          subscriptionEmitter.isSent = true
          if (obj.payload && obj.payload.___$error___) {
            subscriptionEmitter.emit('error', parseError(obj))
          } else {
            subscriptionEmitter.emit('update', obj)
          }
        } else {
          if (connection.selvaSubscriptionEmitters[channel]) {
            connection.selvaSubscriptionEmitters[channel].forEach(
              subscriptionEmitter => {
                subscriptionEmitter.isSent = true
                if (obj.payload && obj.payload.___$error___) {
                  subscriptionEmitter.emit('error', parseError(obj))
                } else {
                  subscriptionEmitter.emit('update', obj)
                }
              }
            )
          }
        }
      } else {
        if (subscriptionEmitter) {
          // can be resend on other stuff
          subscriptionEmitter.isSent = true
        } else if (connection.selvaSubscriptionEmitters[channel]) {
          connection.selvaSubscriptionEmitters[channel].forEach(
            subscriptionEmitter => {
              subscriptionEmitter.isSend = true
            }
          )
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
  connection: Connection,
  channel: string,
  getOptions: GetOptions
) {
  connection.command({
    command: 'hsetnx',
    args: [SUBSCRIPTIONS, channel, JSON.stringify(getOptions)]
  })
  connection.command({
    command: 'sadd',
    args: [channel, connection.uuid]
  })
  connection.command({
    command: 'publish',
    args: [
      NEW_SUBSCRIPTION,
      JSON.stringify({ client: connection.uuid, channel })
    ]
  })
  connection.command({
    command: 'subscribe',
    args: [channel]
  })
}

export function startObserver(
  connection: Connection,
  channel: string,
  subscriptionEmitter: SubscriptionEmitter // will become 'observer'
) {
  if (!connection.selvaSubscriptionEmitters[channel]) {
    connection.selvaSubscriptionEmitters[channel] = new Set()
    sendObserver(connection, channel, subscriptionEmitter.getOptions)
  }
  connection.selvaSubscriptionEmitters[channel].add(subscriptionEmitter)
  getObserverValue(connection, channel, subscriptionEmitter)
}

export function stopObserver(
  connection: Connection,
  channel: string,
  observerEmitter: SubscriptionEmitter
) {
  if (connection.selvaSubscriptionEmitters[channel]) {
    connection.selvaSubscriptionEmitters[channel].delete(observerEmitter)
    if (connection.selvaSubscriptionEmitters[channel].size === 0) {
      connection.command({
        command: 'srem',
        args: [channel, connection.uuid]
      })
      connection.command({
        command: 'publish',
        args: [
          REMOVE_SUBSCRIPTION,
          JSON.stringify({ client: this.uuid, channel })
        ]
      })

      connection.command({ command: 'unsubscribe', args: [channel] })

      delete connection.selvaSubscriptionEmitters[channel]
    }
  }
}

export function startSelvaSubscriptions() {
  // add timer
  // add message handler
}

export function stopSelvaSubscriptions() {
  // remove timer
  // remove message handler
}
