import { ConnectOptions, ServerDescriptor } from '../types'
import { getClient } from './clients'
import RedisSelvaClient from './'
import {
  REGISTRY_UPDATE,
  REGISTRY_UPDATE_STATS,
  REGISTRY_UPDATE_SUBSCRIPTION
} from '../constants'
import { Servers, ServersById } from './types'

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

const getServers = async (client: RedisSelvaClient, id?: string) => {
  // handle specific id!
  console.log('update for ', id)

  delete client.servers
  const serverList =
    (await client.smembers({ type: 'registry' }, 'servers')) || []
  const servers: Servers = {}
  const serversById: ServersById = {}
  const result: ServerDescriptor[] = await Promise.all(
    serverList.map(
      async (id: string): Promise<ServerDescriptor> => {
        const [host, port, name, type, def] = await client.hmget(
          { type: 'registry' },
          id,
          'host',
          'port',
          'name',
          'type',
          'default'
        )
        const descriptor = {
          host,
          port: Number(port),
          name,
          type,
          default: def ? true : false
        }
        serversById[id] = descriptor
        return descriptor
      }
    )
  )
  for (const server of result) {
    if (!servers[server.name]) {
      servers[server.name] = {}
      if (server.default) {
        servers.default = servers[server.name]
      }
    }
    if (!servers[server.name][server.type]) {
      servers[server.name][server.type] = []
    }
    servers[server.name][server.type].push(server)
  }
  client.serversById = serversById
  client.servers = servers
  client.registry.emit('servers_updated', servers)
}

type SubscriptionUpdates = {
  host: string
  port: number
  subscriptions: Record<string, 'removed' | 'created'>
}

const updateSubscriptions = (
  client: RedisSelvaClient,
  subscriptionUpdates: SubscriptionUpdates
) => {
  console.log('mmm yesh', subscriptionUpdates)

  // client.registry.emit('subscription_updated', subscriptionUpdates)
}

const createRegistryClient = (
  client: RedisSelvaClient,
  port: number,
  host: string
) => {
  client.registry = getClient(client, 'registry', 'registry', port, host)
  client.subscribe({ type: 'registry' }, REGISTRY_UPDATE)
  client.on({ type: 'registry' }, 'message', (channel, payload) => {
    if (channel === REGISTRY_UPDATE) {
      // console.log('REGISTRY UPDATED (could be a new client!')
      // start with putting it in here!
      getServers(client, <string>payload)
    } else if (channel === REGISTRY_UPDATE_SUBSCRIPTION) {
      // client.registry.emit('subscriptions_updated', )
      updateSubscriptions(client, <SubscriptionUpdates>JSON.parse(payload))
    }
  })

  getServers(client)
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
    createRegistryClient(client, connectOptions.port, connectOptions.host)
    drainQueue(client)
  }
}

export default connectRegistry
