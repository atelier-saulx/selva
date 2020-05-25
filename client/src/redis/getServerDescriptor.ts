import RedisSelvaClient from './'
import { ServerSelector, ServerDescriptor } from '../types'

const getServerDescriptor = async (
  selvaRedisClient: RedisSelvaClient,
  selector: ServerSelector
): Promise<ServerDescriptor> => {
  const retry = (): Promise<ServerDescriptor> =>
    new Promise(resolve => {
      selvaRedisClient.registry.once('servers_updated', () => {
        resolve(getServerDescriptor(selvaRedisClient, selector))
      })
    })

  if (!selvaRedisClient.servers) {
    return retry()
  }
  if (selector.host && selector.port) {
    const server =
      selvaRedisClient.serversById[`${selector.host}:${selector.port}`]
    if (!server) {
      return retry()
    }

    return server
  }
  if (!selector.name) {
    if (
      selector.type === 'registry' ||
      selector.type === 'subscriptionManager'
    ) {
      selector.name = selector.type
    } else {
      selector.name = 'default'
    }
  }
  if (!selector.type) {
    if (
      selector.name === 'registry' ||
      selector.name === 'subscriptionManager'
    ) {
      selector.type = selector.name
    } else {
      selector.type = 'origin'
    }
  }

  if (
    !selvaRedisClient.servers[selector.name] ||
    !selvaRedisClient.servers[selector.name][selector.type]
  ) {
    if (
      selvaRedisClient.servers[selector.name] &&
      selector.type === 'replica'
    ) {
      selector.type = 'origin'
      return getServerDescriptor(selvaRedisClient, selector)
    }
    return retry()
  }

  const servers = selvaRedisClient.servers[selector.name][selector.type]
  if (selector.type === 'subscriptionManager' && selector.subscription) {
    const channel = selector.subscription
    const subsManagers = selvaRedisClient.subsManagers
    for (let i = 0, len = subsManagers.length; i < len; i++) {
      if (subsManagers[i].subscriptions.has(channel)) {
        return subsManagers[i]
      }
    }
    return subsManagers[0]
  } else {
    return servers[Math.floor(Math.random() * servers.length)]
  }
}

export default getServerDescriptor
