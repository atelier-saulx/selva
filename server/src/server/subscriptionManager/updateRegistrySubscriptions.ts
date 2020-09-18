import { SelvaClient, constants } from '@saulx/selva'
import { SubscriptionManager } from './types'

type Subscriptions = {
  host: string
  port: number
  subscriptions: Record<string, 'created' | 'removed'>
}

let subscriptions: Record<string, 'created' | 'removed'> = {}
let publishInProgress = false

export default async function updateRegistry(
  client: SelvaClient,
  info: Subscriptions,
  subsManager: SubscriptionManager
) {
  console.log('Update subs in registry', info)

  const id = info.host + ':' + info.port

  // UPDATE NUMBER OF SUBS

  const size = Object.keys(subsManager.subscriptions).length

  client.redis.hset({ type: 'registry' }, id, 'subs', size)

  for (let channel in info.subscriptions) {
    const type = info.subscriptions[channel]

    if (type === 'created') {
      await client.redis.sadd(
        { type: 'subscriptionRegistry' },
        constants.REGISTRY_SUBSCRIPTION_INDEX + '_' + id,
        channel
      )
      const prev = await client.redis.get(
        { type: 'subscriptionRegistry' },
        channel
      )
      if (prev) {
        if (prev !== id) {
          console.log('previous!', prev)
          // publish

          // await client.redis.set({ type: 'subscriptionRegistry' }, channel, id)
        } else {
          console.log('allrdy keep')
        }
        // check if server exists
        // if so then send a move publish command to the registry
      } else {
        await client.redis.set({ type: 'subscriptionRegistry' }, channel, id)
      }
    } else {
      await client.redis.srem(
        { type: 'subscriptionRegistry' },
        constants.REGISTRY_SUBSCRIPTION_INDEX + '_' + id,
        channel
      )
      const prev = await client.redis.get(
        { type: 'subscriptionRegistry' },
        channel
      )
      if (prev === id) {
        console.log('remove it')
        await client.redis.del(channel)
      }
    }

    await client.redis.set({ type: 'subscriptionRegistry' }, channel, id)
  }

  const x = await client.redis.smembers(
    { type: 'subscriptionRegistry' },
    constants.REGISTRY_SUBSCRIPTION_INDEX + '_' + id
  )

  console.log(x)

  // console.log('ok this is now a different thingy')
  // // need to find if created etc - have to send the 'removed'
  // // make a constant registry_subscription_index

  // for (let key in info.subscriptions) {
  //   subscriptions[key] = info.subscriptions[key]
  // }
  // if (!publishInProgress) {
  //   publishInProgress = true
  //   const id = info.host + ':' + info.port
  //   process.nextTick(() => {
  //     const q = []
  //     for (let key in subscriptions) {
  //       const event = subscriptions[key]
  //       if (event === 'created') {
  //         q.push(
  //           client.redis.sadd({ type: 'registry' }, `${id}_subscriptions`, key)
  //         )
  //       } else if (event === 'removed') {
  //         q.push(
  //           client.redis.srem({ type: 'registry' }, `${id}_subscriptions`, key)
  //         )
  //       }
  //     }
  //     publishInProgress = false
  //     subscriptions = {}
  //     Promise.all(q).then(() => {
  //       client.redis.publish(
  //         { type: 'registry' },
  //         constants.REGISTRY_UPDATE_SUBSCRIPTION,
  //         JSON.stringify(info)
  //       )
  //     })
  //   })
  // }
}
