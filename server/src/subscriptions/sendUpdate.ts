import { prefixes } from '@saulx/selva'
import { createHash } from 'crypto'
import { updateQueries as updateNowQueries } from './now'
import { QuerySubscription } from './'
import SubscriptionManager from './subsManager'

const sendUpdate = async (
  subscriptionManager: SubscriptionManager,
  subscriptionId: string,
  deleteOp: boolean = false
) => {
  if (!subscriptionManager.pub) {
    return
  }

  const subscription = subscriptionManager.subscriptions[subscriptionId]

  if (!subscription) {
    console.error(
      `Cannot find subscription on server ${subscriptionId.slice(-5)}`
    )
    return
  }

  if (deleteOp) {
    const event = JSON.stringify({ type: 'delete' })
    await Promise.all([
      subscriptionManager.client.redis.byType.hset(
        'sClient',
        prefixes.cache,
        subscriptionId,
        event
      ),
      subscriptionManager.client.redis.byType.hset(
        'sClient',
        prefixes.cache,
        subscriptionId + '_version',
        ''
      )
    ])
    await subscriptionManager.client.redis.byType.publish(
      'sClient',
      subscriptionId,
      ''
    )
    return
  }

  const getOptions = subscriptionManager.subscriptions[subscriptionId].get

  const payload = await subscriptionManager.client.get(
    Object.assign({}, getOptions, {
      $includeMeta: true
    })
  )

  // handle refs -- add this somewhere else
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
  if (hasRefs) {
    // FIXME: very slow to do this all the time for everything :/
    console.log('WARNING UPDATING ALL SUBS BECAUSE OF REF CHANGE (SLOW!)')
    // will go into an endless loop scince creation of subscriptions call sendupdate
    // subscriptionManager.updateSubscriptionData(true)
  }

  // handle query

  // need to add some stuff here

  // if has inherits

  // make query

  console.log('----->', payload)

  if (payload.$meta.inherit) {
    console.log('GO', payload.$meta)
    if (!payload.$meta.query) {
      payload.$meta.query = []
    }

    for (let key in payload.$meta.inherit) {
      let hasAncestor = false
      for (let k in payload.$meta.inherit[key].ids) {
        hasAncestor = true
        break
      }
      if (hasAncestor) {
        payload.$meta.inherit[key].idFields = { [`${key}.ancestors`]: true }
      }
      payload.$meta.inherit[key].ids[key] = true
      console.dir(payload.$meta.inherit[key], { depth: 10 })
      payload.$meta.query.push(payload.$meta.inherit[key])
    }
  }

  if (payload.$meta.query) {
    subscriptionManager.queries[subscriptionId] = <QuerySubscription[]>(
      payload.$meta.query
    )
    console.dir(payload.$meta.query, { depth: 10 })
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

  // dont encode/decode as many times
  const resultStr = JSON.stringify({ type: 'update', payload })
  const currentHash = subscription.version
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
  subscription.version = newHash
  // update cache

  // also do this on intial
  await Promise.all([
    subscriptionManager.client.redis.byType.hset(
      'sClient',
      prefixes.cache,
      subscriptionId,
      resultStr
    ),
    subscriptionManager.client.redis.byType.hset(
      'sClient',
      prefixes.cache,
      subscriptionId + '_version',
      newHash
    )
  ])

  subscriptionManager.client.redis.byType.publish(
    'sClient',
    subscriptionId,
    newHash
  )
}

export default sendUpdate
