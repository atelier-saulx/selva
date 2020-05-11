import { ConnectOptions } from '../types'
import { getClient } from './clients'
import RedisSelvaClient from './'

const drainQueue = (client: RedisSelvaClient) => {
  client.queue.forEach(({ command, selector }) => {
    client.addCommandToQueue(command, selector)
  })
  client.listenerQueue.forEach(({ event, callback, selector }) => {
    client.on(selector, event, callback)
  })
  client.listenerQueue = []
  client.queue = []
}

const connectRegistry = (
  client: RedisSelvaClient,
  connectOptions: ConnectOptions
) => {
  if (typeof connectOptions === 'function') {
    let prevConnectOptions
    connectOptions().then(parsedConnectOptions => {
      console.log('hello!')
      prevConnectOptions = parsedConnectOptions
      client.registry = getClient(
        client,
        'registry',
        'registry',
        parsedConnectOptions.port,
        parsedConnectOptions.host
      )
      client.registry.once('disconnect', () => {
        console.log('o o maybe changed - registry is dc!')
      })
      drainQueue(client)
    })
  } else if (connectOptions instanceof Promise) {
    connectOptions.then(parsedConnectOptions => {
      client.registry = getClient(
        client,
        'registry',
        'registry',
        parsedConnectOptions.port,
        parsedConnectOptions.host
      )
      drainQueue(client)
    })
  } else {
    console.log('start with non async connect')
    client.registry = getClient(
      client,
      'registry',
      'registry',
      connectOptions.port,
      connectOptions.host
    )
    drainQueue(client)
  }
}

export default connectRegistry
