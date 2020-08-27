import { SelvaClient, constants } from '@saulx/selva'
import { RegistryInfo } from '../types'

export default async function updateRegistry(
  client: SelvaClient,
  info: RegistryInfo
) {
  const args = []

  for (let key in info) {
    if (key === 'stats') {
      args.push(key, JSON.stringify(info[key]))
    } else {
      args.push(key, info[key])
    }
  }

  const id = info.host + ':' + info.port

  const isNew = !(await client.redis.sismember(
    { type: 'registry' },
    'servers',
    id
  ))

  await Promise.all([
    client.redis.sadd({ type: 'registry' }, 'servers', id),
    client.redis.hmset({ type: 'registry' }, id, ...args)
  ])

  if (info.stats) {
    client.redis.publish(
      { type: 'registry' },
      constants.REGISTRY_UPDATE_STATS,
      id
    )

    if (isNew) {
      client.redis.publish({ type: 'registry' }, constants.REGISTRY_UPDATE, id)
    }
  } else {
    client.redis.publish({ type: 'registry' }, constants.REGISTRY_UPDATE, id)
  }
}
