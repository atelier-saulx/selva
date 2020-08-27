import { SelvaClient } from '..'
import { GetOptions, GetResult } from '../get/types'
import Observable from './observable'
import { createHash } from 'crypto'

function makeSubscriptionId(opts: GetOptions) {
  const hash = createHash('sha256')
  hash.update(JSON.stringify(opts))
  return hash.digest('hex')
}

export function observe(
  client: SelvaClient,
  props: GetOptions
): Observable<GetResult> {
  // do validation here
  // props
  const subscriptionId = makeSubscriptionId(props)
  const channel = `___selva_subscription:${subscriptionId}`
  const observable = client.redis.observe(channel, props)
  return observable
}

export function observeSchema(client: SelvaClient, dbName: string) {
  const obs = client.redis.observe(
    `___selva_subscription:schema_update:${dbName}`,
    {}
  )

  return obs
}
