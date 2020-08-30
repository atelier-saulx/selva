import { SelvaClient, constants } from '@saulx/selva'
import { RegistryInfo } from '../types'

export async function addSubscriptionToRegistry() {
  // needs to write the new subscription in a special field (maybe not)
  // and needs to check the reverse map and send an update for that
  // when all this is done remove status from cms hub for now....
}

export default async function updateRegistry(
  client: SelvaClient,
  info: RegistryInfo
) {
  const args = []

  // just remove subscriptions from this

  for (let key in info) {
    if (key === 'stats') {
      args.push(key, JSON.stringify(info[key]))
    } else {
      args.push(key, info[key])
    }
  }

  console.log('Yo update dat registry', info)

  const id = info.host + ':' + info.port

  const isNew = !(await client.redis.sismember(
    { type: 'registry' },
    'servers',
    id
  ))

  // if new server

  await Promise.all([
    client.redis.sadd({ type: 'registry' }, 'servers', id),

    // remove subs here
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
