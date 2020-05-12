import { ConnectOptions } from '../types'
import { getClient } from './clients'
import RedisSelvaClient from './'
import { REGISTRY_UPDATE } from '../constants'

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

const createRegistryClient = (
  client: RedisSelvaClient,
  port: number,
  host: string
) => {
  client.registry = getClient(client, 'registry', 'registry', port, host)
  client.subscribe({ type: 'registry' }, REGISTRY_UPDATE)
  // view of the registry if its not there async fn needs to wait for it
  // this.registry

  client.on({ type: 'registry' }, 'message', () => {
    console.log('REGISTRY UPDATED (could be a new client!')
  })
}

const connectRegistry = (
  client: RedisSelvaClient,
  connectOptions: ConnectOptions
) => {
  if (typeof connectOptions === 'function') {
    let prevConnectOptions
    connectOptions().then(parsedConnectOptions => {
      prevConnectOptions = parsedConnectOptions
      createRegistryClient(
        client,
        parsedConnectOptions.port,
        parsedConnectOptions.host
      )
      const dcHandler = async () => {
        const newConnectionOptions = await connectOptions()
        if (
          newConnectionOptions.host !== prevConnectOptions.host ||
          newConnectionOptions.port !== prevConnectOptions.port
        ) {
          client.registry.removeListener('disconnect', dcHandler)
          client.registry = undefined
          connectRegistry(client, connectOptions)
        }
      }
      client.registry.on('disconnect', dcHandler)
      drainQueue(client)
    })
  } else if (connectOptions instanceof Promise) {
    connectOptions.then(parsedConnectOptions => {
      createRegistryClient(
        client,
        parsedConnectOptions.port,
        parsedConnectOptions.host
      )
      drainQueue(client)
    })
  } else {
    console.log('start with non async connect')
    createRegistryClient(client, connectOptions.port, connectOptions.host)
    drainQueue(client)
  }
}

export default connectRegistry
