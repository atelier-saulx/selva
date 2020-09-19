import { SelvaClient, constants } from '@saulx/selva'
import { SubscriptionManager } from './types'

type Subscriptions = {
  host: string
  port: number
  subscriptions: Record<string, 'created' | 'removed'>
}

let subscriptions: Record<string, 'created' | 'removed'> = {}
let publishInProgress = false

const handleAddPrev = async (
  client: SelvaClient,
  channel: string,
  id: string
) => {
  const prev = await client.redis.get({ type: 'subscriptionRegistry' }, channel)
  if (prev) {
    if (prev !== id) {
      console.log('previous id SUBS MANAGER need to override', prev)
      // publish
      // await client.redis.set({ type: 'subscriptionRegistry' }, channel, id)
    } else {
      console.log('allrdy have subs (sm update reg) keep')
    }
  } else {
    await client.redis.set({ type: 'subscriptionRegistry' }, channel, id)
  }
}

const handleRemovePrev = async (
  client: SelvaClient,
  channel: string,
  id: string
) => {
  const prev = await client.redis.get({ type: 'subscriptionRegistry' }, channel)
  if (prev === id) {
    console.log('remove it')
    await client.redis.del(channel)
  }
}

export default async function updateRegistry(
  client: SelvaClient,
  info: Subscriptions,
  subsManager: SubscriptionManager
) {
  for (let key in info.subscriptions) {
    subscriptions[key] = info.subscriptions[key]
  }

  if (!publishInProgress) {
    publishInProgress = true
    process.nextTick(() => {
      console.log('ok process', subscriptions)
      const q = []
      const id = info.host + ':' + info.port
      const size = Object.keys(subsManager.subscriptions).length
      q.push(client.redis.hset({ type: 'registry' }, id, 'subs', size))

      for (let channel in subscriptions) {
        // make this efficient wiht a q
        const type = subscriptions[channel]
        if (type === 'created') {
          q.push(
            client.redis.sadd(
              { type: 'subscriptionRegistry' },
              constants.REGISTRY_SUBSCRIPTION_INDEX + '_' + id,
              channel
            )
          )
          q.push(handleAddPrev(client, channel, id))
          // q.push(
          //   client.redis.set({ type: 'subscriptionRegistry' }, channel, id)
          // )
        } else if (type === 'removed') {
          q.push(
            client.redis.srem(
              { type: 'subscriptionRegistry' },
              constants.REGISTRY_SUBSCRIPTION_INDEX + '_' + id,
              channel
            )
          )
          q.push(handleRemovePrev(client, channel, id))
        }
      }

      publishInProgress = false
      subscriptions = {}
      Promise.all(q)
    })
  }
}
