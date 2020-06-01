import { SelvaClient } from '..'
import { GetOptions, GetResult } from '../get/types'
import Observable from './observable'
import { createHash } from 'crypto'
import { Schema } from '~selva/schema'

function makeSubscriptionId(opts: GetOptions) {
  const hash = createHash('sha256')
  hash.update(JSON.stringify(opts))
  return hash.digest('hex')
}

export function observe(
  client: SelvaClient,
  props: GetOptions
): Observable<GetResult> {
  const subscriptionId = makeSubscriptionId(props)
  const channel = `___selva_subscription:${subscriptionId}`
  let cached: boolean = false

  const observable = client.redis.observe(channel, props)

  return new Observable<GetResult>(observe => {
    const sub = observable.subscribe({
      next: (x: GetResult) => {
        cached = false
        observe.next(x)
      },
      error: observe.error,
      complete: observe.complete
    })
    return <any>sub
  })
}

export function observeSchema(client: SelvaClient, dbName: string) {
  const obs = client.redis.observe(
    `___selva_subscription:schema_update:${dbName}`,
    {}
  )

  return new Observable<Schema>(observe => {
    const sub = obs.subscribe({
      next: (_x: any) => {
        observe.next(_x)
      },
      error: observe.error,
      complete: observe.complete
    })

    return <any>sub
  })
}
