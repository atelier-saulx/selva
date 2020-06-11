import { ConnectOptions, ServerDescriptor } from '../types'
import { getClient, destroyClient } from './clients'
import RedisSelvaClient from './'
import {
  REGISTRY_UPDATE,
  REGISTRY_UPDATE_STATS,
  REGISTRY_UPDATE_SUBSCRIPTION
} from '../constants'
import { Servers, ServersById } from './types'
import { subsmanagerRemoved } from './observers'

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
  const serverList = (
    (await client.smembers({ type: 'registry' }, 'servers')) || []
  ).filter(v => !!v)
  const servers: Servers = {}
  const serversById: ServersById = {}
  const subManagerObj: Record<string, true> = {}
  const subsManagers = []

  const result: ServerDescriptor[] = await Promise.all(
    serverList.map(
      async (id: string): Promise<ServerDescriptor> => {
        const [host, port, name, type, def, stats] = await client.hmget(
          { type: 'registry' },
          id,
          'host',
          'port',
          'name',
          'type',
          'default',
          'stats'
        )

        const descriptor: ServerDescriptor = {
          host,
          port: Number(port),
          name,
          type,
          stats: stats ? JSON.parse(stats) : {},
          default: def ? true : false
        }

        if (type === 'subscriptionManager') {
          const subsKey = `${id}_subscriptions`
          const subs = await client.smembers({ type: 'registry' }, subsKey)
          descriptor.subscriptions = new Set(subs || [])
          subsManagers.push(descriptor)
          subManagerObj[id] = true
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

  // need to check for diff

  subsManagers.sort(sortSubsManagers)

  if (client.subsManagers && client.subsManagers.length) {
    for (let i = 0; i < client.subsManagers.length; i++) {
      const id = `${client.subsManagers[i].host}:${client.subsManagers[i].port}`
      if (!subManagerObj[id]) {
        subsmanagerRemoved(client, id)
      }
    }
  }

  client.subsManagers = subsManagers
  client.serversById = serversById
  client.servers = servers
  client.registry.emit('servers_updated', servers)
  client.selvaClient.emit('servers_updated', servers)
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
        if (!server.subscriptions) {
          server.subscriptions = new Set()
        }

        server.subscriptions.add(channel)
      } else if (subscriptions[channel] === 'removed' && server.subscriptions) {
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
  clearTimeout(client.timeoutServers)

  const descriptor: ServerDescriptor = {
    port,
    host,
    name: 'registry',
    type: 'registry'
  }

  client.registry = getClient(client, descriptor)

  client.registry.on('connect', () => {
    client.selvaClient.emit('connect')
  })

  client.registry.on('disconnect', () => {
    client.selvaClient.emit('disconnect')
  })

  client.subscribe(descriptor, REGISTRY_UPDATE)
  client.subscribe(descriptor, REGISTRY_UPDATE_SUBSCRIPTION)

  if (client.selvaClient.serverType === 'registry') {
    client.subscribe(descriptor, REGISTRY_UPDATE_STATS)
  }

  const setTimeoutServer = () => {
    clearTimeout(client.timeoutServers)
    client.timeoutServers = setTimeout(() => {
      getServers(client)
    }, 30e3)
  }

  setTimeoutServer()

  client.on({ type: 'registry' }, 'message', (channel, payload) => {
    if (
      channel === REGISTRY_UPDATE ||
      (client.selvaClient.serverType === 'registry' &&
        channel === REGISTRY_UPDATE_STATS)
    ) {
      setTimeoutServer()
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
          destroyClient(client.registry)
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
