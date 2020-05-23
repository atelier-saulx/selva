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

const sortSubsManagers = (a, b) => {
  a = a.subscriptions.size
  b = b.subscriptions.size
  return a > b ? 1 : b > a ? -1 : 0
}

const getServers = async (client: RedisSelvaClient, id?: string) => {
  delete client.servers
  const serverList =
    (await client.smembers({ type: 'registry' }, 'servers')) || []
  const servers: Servers = {}
  const serversById: ServersById = {}
  const subsManagers = []
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
        const descriptor: ServerDescriptor = {
          host,
          port: Number(port),
          name,
          type,
          default: def ? true : false
        }

        if (type === 'subscriptionManager') {
          const subsKey = `${id}_subscriptions`
          const subs = await client.smembers({ type: 'registry' }, subsKey)
          descriptor.subscriptions = new Set(subs || [])
          subsManagers.push(descriptor)
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

  subsManagers.sort(sortSubsManagers)

  client.subsManagers = subsManagers
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
  if (!client.servers) {
    return
  }
  const { host, port, subscriptions } = subscriptionUpdates
  const id = host + ':' + port
  const server = client.serversById[id]
  if (server) {
    for (let channel in subscriptions) {
      if (subscriptions[channel] === 'created') {
        server.subscriptions.add(channel)
      } else if (subscriptions[channel] === 'removed') {
        server.subscriptions.delete(channel)
      }
    }
    client.subsManagers.sort(sortSubsManagers)
  }
  // does not matter that much but may ne nice
  // client.registry.emit('subscription_updated', subscriptionUpdates)
}

const createRegistryClient = (
  client: RedisSelvaClient,
  port: number,
  host: string
) => {
  client.registry = getClient(client, {
    port,
    host,
    name: 'registry',
    type: 'registry'
  })

  client.registry.on('connect', () => {
    client.selvaClient.emit('connect')
  })

  client.subscribe({ type: 'registry' }, REGISTRY_UPDATE)
  client.subscribe({ type: 'registry' }, REGISTRY_UPDATE_SUBSCRIPTION)

  client.on({ type: 'registry' }, 'message', (channel, payload) => {
    if (channel === REGISTRY_UPDATE) {
      // can be handled more effiecently
      getServers(client, <string>payload)
    } else if (channel === REGISTRY_UPDATE_SUBSCRIPTION) {
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
