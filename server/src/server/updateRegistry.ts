import { SelvaClient, constants } from '@saulx/selva'
import { RegistryInfo } from '../types'
import { SelvaServer } from './'

export async function removeFromRegistry(client: SelvaClient) {
  console.log(client.server)

  const redis = client.redis
  const id = `${client.server.host}:${client.server.port}`

  console.log('REMOVE')

  await Promise.all([
    redis.srem({ type: 'registry' }, 'servers', id),
    redis.del({ type: 'registry' }, id)
  ])

  await redis.publish(
    { type: 'registry' },
    constants.REGISTRY_UPDATE,
    JSON.stringify({
      event: 'remove',
      server: client.server
    })
  )
}

const block = (server: SelvaServer): boolean => {
  const isBlocked = !server.pm || server.pm.isDestroyed || server.isDestroyed
  if (isBlocked) console.log('IS BLOCKED', server.type)
  return isBlocked
}

export default async function updateRegistry(
  server: SelvaServer,
  info: RegistryInfo
) {
  if (block(server)) {
    return
  }

  const client = server.selvaClient

  const args = []

  // just remove subscriptions from this

  for (let key in info) {
    if (key === 'stats') {
      args.push(key, JSON.stringify(info[key]))
    } else {
      args.push(key, info[key])
    }
  }

  const id = info.host + ':' + info.port

  // console.log('yesh want to make update times', info)

  const isNew = !(await client.redis.sismember(
    { type: 'registry' },
    'servers',
    id
  ))

  if (block(server)) {
    return
  }

  // hget only fields you need
  await Promise.all([
    client.redis.sadd({ type: 'registry' }, 'servers', id),

    // remove subs here
    client.redis.hmset({ type: 'registry' }, id, ...args)
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
          type: info.type
        }
      })
    )
  }
}
