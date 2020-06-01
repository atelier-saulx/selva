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

  // props optional
  if (client.redis.observables[channel]) {
    cached = true
  }

  const observable = client.redis.observe(channel, props)

  return new Observable<GetResult>(observe => {
    if (cached) {
      client.get(props).then(result => {
        // check if we still need the initial event or if latest is not anymore "cached"
        if (cached) {
          observe.next(result)
        }
      })
    }

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
