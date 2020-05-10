import { SelvaClient } from '@saulx/selva'
import { RegistryInfo } from '../types'

export default async function updateRegistry(
  client: SelvaClient,
  info: RegistryInfo,
  host: string,
  port: number
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

  return client.redis.hmset(
    {
      name: 'registry',
      type: 'registry'
    },
    ...args
  )
}
