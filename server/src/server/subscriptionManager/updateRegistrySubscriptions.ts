import { SelvaClient, constants } from '@saulx/selva'
import { SubscriptionManager } from './types'
import chalk from 'chalk'

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
      await client.redis.set({ type: 'subscriptionRegistry' }, channel, id)
      // server has to do removal etc
      const [host, p] = prev.split(':')

      const port = Number(p)

      if (
        client.servers.subsManagers.find(
          (s) => s.port === port && s.host === host
        )
      ) {
        // if the server is unregistered this will be useless to add to a quuee
        await client.redis.publish(
          { host, port: port, type: 'subscriptionManager' },
          constants.REGISTRY_MOVE_SUBSCRIPTION,
          JSON.stringify([channel, id])
        )
      } else {
        // this is a bit hard if it is not synced yet something can go wrong here
        console.info(
          chalk.yellow(
            `Cannot find previous server to move subscription ${prev} has to move to ${id}`
          )
        )
      }
    } else {
      // console.log(
      //   chalk.gray(
      //     `Allready have subscription ${channel} on ${id} don't do anything`
      //   )
      // )
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
    await client.redis.del({ type: 'subscriptionRegistry' }, channel)
  }
}

export default async function updateRegistry(
  client: SelvaClient,
  info: Subscriptions,
  subsManager: SubscriptionManager
) {
  for (const key in info.subscriptions) {
    subscriptions[key] = info.subscriptions[key]
  }

  if (!publishInProgress) {
    publishInProgress = true
    process.nextTick(() => {
      const q = []
      const id = info.host + ':' + info.port
      const size = Object.keys(subsManager.subscriptions).length
      q.push(client.redis.hset({ type: 'registry' }, id, 'subs', size))
      for (const channel in subscriptions) {
        // make this efficient wiht a q
        const type = subscriptions[channel]
        if (type === 'created') {
          q.push(
            client.redis.sadd(
              { type: 'subscriptionRegistry' },
              constants.REGISTRY_SUBSCRIPTION_INDEX + id,
              channel
            )
          )
          q.push(handleAddPrev(client, channel, id))
        } else if (type === 'removed') {
          q.push(
            client.redis.srem(
              { type: 'subscriptionRegistry' },
              constants.REGISTRY_SUBSCRIPTION_INDEX + id,
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
