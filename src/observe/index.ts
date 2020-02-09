import { SelvaClient } from '..'
import { GetOptions, GetResult } from '../get/types'
import Observable from './observable'
import { v4 as uuid } from 'uuid'

type ObserveOptions = {
  getLatest: boolean
}

type UpdateEvent = {
  type: 'update'
  payload: GetResult
}

type Event = UpdateEvent

export async function observe(
  client: SelvaClient,
  props: GetOptions,
  opts: ObserveOptions = { getLatest: true }
): Promise<Observable<GetResult>> {
  const subscriptionId = uuid()
  await client.redis.hset(
    '___selva_subscriptions',
    subscriptionId,
    JSON.stringify(props)
  )

  const obs = client.redis.subscribe(`___selva_subscription:${subscriptionId}`)
  return new Observable<GetResult>(observe => {
    const sub = obs.subscribe({
      next: (x: string) => {
        const event: Event = JSON.parse(x)

        if (event.type === 'update') {
          observe.next(event.payload)
        }
      },
      error: observe.error,
      complete: observe.complete
    })

    return <any>sub
  })
}
