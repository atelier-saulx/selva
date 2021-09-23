import { SelvaClient, constants } from '@saulx/selva'
import { RegistryInfo } from '../types'
import ProcessManager from './processManager'

type RegisterableServer = {
  selvaClient: SelvaClient
  pm: ProcessManager
  isDestroyed?: boolean
}

export async function removeFromRegistry(client: SelvaClient) {
  const redis = client.redis
  const id = `${client.server.host}:${client.server.port}`
  await redis.hdel({ type: 'timeseriesRegistry' }, 'servers', id)

  await redis.publish(
    { type: 'timeseriesRegistry' },
    constants.TS_REGISTRY_UPDATE,
    JSON.stringify({
      event: 'remove',
      server: client.server,
    })
  )
}

const block = (server: RegisterableServer): boolean => {
  const isBlocked = !server.pm || server.pm.isDestroyed || server.isDestroyed
  return isBlocked
}

export default async function updateRegistry(
  server: RegisterableServer,
  info: RegistryInfo
) {
  if (block(server)) {
    return
  }

  const client = server.selvaClient

  const id = info.host + ':' + info.port

  const isNew = !(await client.redis.hexists(
    { type: 'timeseriesRegistry' },
    'servers',
    id
  ))

  if (block(server)) {
    return
  }

  await client.redis.hset(
    { type: 'timeseriesRegistry' },
    'servers',
    id,
    JSON.stringify(info)
  )

  if (block(server)) {
    return
  }

  if (isNew) {
    // better codes
    client.redis.publish(
      { type: 'timeseriesRegistry' },
      constants.TS_REGISTRY_UPDATE,
      // maybe not nessecary to send all (?)
      JSON.stringify({
        event: 'new',
        server: {
          port: info.port,
          name: info.name,
          host: info.host,
          type: info.type,
        },
      })
    )
  } else {
    client.redis.publish(
      { type: 'timeseriesRegistry' },
      constants.TS_REGISTRY_UPDATE,
      // maybe not nessecary to send all (?)
      JSON.stringify({
        event: 'update',
        server: {
          port: info.port,
          name: info.name,
          host: info.host,
          type: info.type,
        },
      })
    )
  }
}
