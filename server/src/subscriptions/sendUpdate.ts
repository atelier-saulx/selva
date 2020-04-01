import { GetOptions } from '@saulx/selva'
import { createHash } from 'crypto'
import { updateQueries as updateNowQueries } from './now'
import { QuerySubscription } from './'
import SubscriptionManager from './index'

const sendUpdate = async (
  subscriptionManager: SubscriptionManager,
  subscriptionId: string,
  getOptions?: GetOptions,
  deleteOp: boolean = false
) => {
  if (!subscriptionManager.pub) {
    return
  }

  if (deleteOp) {
    subscriptionManager.pub.publish(
      `___selva_subscription:${subscriptionId}`,
      JSON.stringify({ type: 'delete' }),
      (err, _reply) => {
        if (err) {
          console.error(err)
        }
      }
    )

    // delete cache for latest result since there is no result now!
    // delete subscriptionManager.lastResultHash[subscriptionId]
    return
  }

  getOptions =
    getOptions || subscriptionManager.subscriptions[subscriptionId].get

  const payload = await subscriptionManager.client.get(
    Object.assign({}, getOptions, {
      $includeMeta: true
    })
  )

  const refs = payload.$meta.$refs

  delete subscriptionManager.refsById[getOptions.$id]

  let hasRefs = false

  const newRefs: Record<string, string> = {}

  for (const refSource in refs) {
    hasRefs = true
    const refTargets = refs[refSource]
    newRefs[refSource] = refTargets
  }
  subscriptionManager.refsById[getOptions.$id] = newRefs

  // what to do here ???
  if (hasRefs) {
    // FIXME: REALLY HAS TO GO
    console.log('WARNING UPDATING ALL SUBS BECAUSE OF REF CHANGE (SLOW!)')
    // very slow to do this all the time
    subscriptionManager.updateSubscriptionData()
  }

  if (payload.$meta.query) {
    subscriptionManager.queries[subscriptionId] = <QuerySubscription[]>(
      payload.$meta.query
    )
    for (const queryMeta of payload.$meta.query) {
      if (queryMeta.time) {
        updateNowQueries(subscriptionManager, {
          subId: subscriptionId,
          nextRefresh: queryMeta.time.nextRefresh
        })
      }
    }
  }

  // clean up $meta before we send it to the client
  // if nested meta remove them
  delete payload.$meta

  // hack-ish thing: include the result object in the string
  // so we don't need to encode/decode as many times
  const resultStr = JSON.stringify({ type: 'update', payload })
  // can start with a cache store for this allready  -- why not

  const currentHash = subscriptionManager.lastResultHash[subscriptionId]
  const hashingFn = createHash('sha256')
  hashingFn.update(resultStr)
  const newHash = hashingFn.digest('hex')

  // de-duplicate events
  // with this we can avoid sending events where nothing changed upon reconnection
  // both for queries and for gets by id
  if (currentHash && currentHash === newHash) {
    return
  }

  // change all this result hash
  subscriptionManager.lastResultHash[subscriptionId] = newHash

  // add publish in the redis client
  console.log('PUBLISH', subscriptionId)
  subscriptionManager.client.redis.publish(subscriptionId, resultStr)
}

export default sendUpdate
