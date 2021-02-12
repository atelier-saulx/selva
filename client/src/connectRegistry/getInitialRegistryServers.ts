import { SelvaClient } from '..'
import { ServerDescriptor } from '../types'
import addServer from './addServer'

const getInitialRegistryServers = async (selvaClient: SelvaClient) => {
  const serverList =
    (await selvaClient.redis.smembers({ type: 'registry' }, 'servers')) || []

  await Promise.all(
    serverList.map(
      async (id: string): Promise<void> => {
        const [host, port, name, type, index] = await selvaClient.redis.hmget(
          { type: 'registry' },
          id,
          'host',
          'port',
          'name',
          'type',
          'index'
        )
        // don't get the stats!
        const server: ServerDescriptor = {
          host,
          port: Number(port),
          name,
          type,
          index: Number(index),
        }
        addServer(selvaClient, <ServerDescriptor>server)
      }
    )
  )
}

export default getInitialRegistryServers
