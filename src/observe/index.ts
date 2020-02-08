import { SelvaClient } from '..'
import { GetOptions, GetResult } from '../get/types'
import Observable from './observable'
import { v4 as uuid } from 'uuid'

type ObserveOptions = {
  getLatest: boolean
}

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
        observe.next(<GetResult>JSON.parse(x))
      },
      error: observe.error,
      complete: observe.complete
    })

    return <any>sub
  })
}
