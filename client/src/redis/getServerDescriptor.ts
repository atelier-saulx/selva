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
    if (selector.type === 'registry') {
      selector.name = 'registry'
    } else {
      selector.name = 'default'
    }
  }
  if (!selector.type) {
    if (selector.name === 'registry') {
      selector.type = 'registry'
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
  const server = servers[Math.floor(Math.random() * servers.length)]

  return server
}

export default getServerDescriptor
