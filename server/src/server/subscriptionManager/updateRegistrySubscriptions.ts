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
    // make this efficient wiht a q
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
          console.log('previous id SUBS MANAGER need to override', prev)
          // publish

          // await client.redis.set({ type: 'subscriptionRegistry' }, channel, id)
        } else {
          console.log('allrdy have subs (sm update reg) keep')
        }
      } else {
        await client.redis.set({ type: 'subscriptionRegistry' }, channel, id)
      }
    } else if (type === 'removed') {
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

  const servers = await client.redis.keys(
    { type: 'subscriptionRegistry' },
    constants.REGISTRY_SUBSCRIPTION_INDEX + '*'
  )

  // for information
  for (let k of servers) {
    const x = await client.redis.smembers({ type: 'subscriptionRegistry' }, k)
    console.log(
      '  subs manager server amount of subscriptions -> ',
      k.replace(constants.REGISTRY_SUBSCRIPTION_INDEX, ''),
      x.length
    )
  }

  // do similair things with the process on next tick

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
