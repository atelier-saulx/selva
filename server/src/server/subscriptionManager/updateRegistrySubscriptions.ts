import { SelvaClient, constants } from '@saulx/selva'

type Subscriptions = {
  host: string
  port: number
  subscriptions: Record<string, 'created' | 'removed'>
}

let subscriptions: Record<string, 'created' | 'removed'> = {}
let publishInProgress = false

export default function updateRegistry(
  client: SelvaClient,
  info: Subscriptions
) {
  for (let key in info.subscriptions) {
    subscriptions[key] = info.subscriptions[key]
  }
  if (!publishInProgress) {
    publishInProgress = true
    const id = info.host + ':' + info.port
    process.nextTick(() => {
      const q = []
      for (let key in subscriptions) {
        const event = subscriptions[key]
        if (event === 'created') {
          q.push(
            client.redis.sadd({ type: 'registry' }, `${id}_subscriptions`, key)
          )
        } else if (event === 'removed') {
          q.push(
            client.redis.srem({ type: 'registry' }, `${id}_subscriptions`, key)
          )
        }
      }
      publishInProgress = false
      subscriptions = {}
      Promise.all(q).then(() => {
        client.redis.publish(
          { type: 'registry' },
          constants.REGISTRY_UPDATE_SUBSCRIPTION,
          JSON.stringify(info)
        )
      })
    })
  }
}
