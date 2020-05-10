import { SelvaClient } from '@saulx/selva'
import { RegistryInfo } from '../types'

export default async function updateRegistry(
  client: SelvaClient,
  info: RegistryInfo
) {
  console.log('write to registry', info)

  const args = []

  for (let key in info) {
    if (key === 'stats') {
      args.push(key, JSON.stringify(info[key]))
    } else if (key === 'subscriptions') {
      args.push(key, JSON.stringify([...info[key].values()]))
    } else {
      args.push(key, JSON.stringify(info[key]))
    }
  }
  const id = info.host + ':' + info.port

  return Promise.all([
    client.redis.sadd({ type: 'registry' }, 'servers', id),
    client.redis.hmset({ type: 'registry' }, id, ...args)
  ])
}
