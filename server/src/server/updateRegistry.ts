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

  await Promise.all([
    redis.srem({ type: 'registry' }, 'servers', id),
    redis.del({ type: 'registry' }, id),
  ])

  await redis.publish(
    { type: 'registry' },
    constants.REGISTRY_UPDATE,
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

  const args = []

  // just remove subscriptions from this

  for (const key in info) {
    if (key === 'stats') {
      args.push(key, JSON.stringify(info[key]))
    } else {
      args.push(key, info[key])
    }
  }

  const id = info.host + ':' + info.port

  console.info('yesh want to make update times', info)

  const isNew = !(await client.redis.sismember(
    { type: 'registry' },
    'servers',
    id
  ))

  console.info('yesh want to make update times', 'IS NEW', isNew)

  if (block(server)) {
    return
  }

  // hget only fields you need
  await Promise.all([
    client.redis.sadd({ type: 'registry' }, 'servers', id),

    // remove subs here
    client.redis.hmset({ type: 'registry' }, id, ...args),
  ])

  if (block(server)) {
    return
  }

  if (info.stats) {
    client.redis.publish(
      { type: 'registry' },
      constants.REGISTRY_UPDATE_STATS,
      id
    )
  }

  if (isNew) {
    console.info('DO IT SEND REG UPDATE ???', info.port, info.type, info.name)

    // better codes
    client.redis.publish(
      { type: 'registry' },
      constants.REGISTRY_UPDATE,
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
  }
}
