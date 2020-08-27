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

  // console.log('yesh want to make update times', info)

  const isNew = !(await client.redis.sismember(
    { type: 'registry' },
    'servers',
    id
  ))

  // hget only fields you need
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
