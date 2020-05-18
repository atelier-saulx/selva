import { constants } from '@saulx/selva'
import { addSubscriptionToTree, removeSubscriptionFromTree } from '../tree'
import { hash } from '../util'
import { Subscription, SubscriptionManager } from '../types'

const { CACHE } = constants

const sendUpdate = async (
  subscriptionManager: SubscriptionManager,
  subscription: Subscription
) => {
  const channel = subscription.channel
  const { client, selector } = subscriptionManager
  const redis = client.redis

  const getOptions = subscription.get
  getOptions.$includeMeta = true

  // SCHEMA UPDATES

  console.log('snurky', getOptions)

  const payload = await client.get(JSON.parse(JSON.stringify(getOptions)))

  // call $meta tree
  const newTree = payload.$meta

  delete payload.$meta

  // make this without payload
  const resultStr = JSON.stringify({ type: 'update', payload })
  const currentVersion = subscription.version
  const newVersion = hash(resultStr)

  const treeVersion = subscription.treeVersion
  const q = []

  if (newTree) {
    const newTreeJson = JSON.stringify(newTree)
    const newTreeVersion = hash(newTreeJson)
    if (treeVersion !== newTreeVersion) {
      if (treeVersion) {
        removeSubscriptionFromTree(subscriptionManager, subscription)
      }
      subscription.treeVersion = newTreeVersion
      subscription.tree = newTree
      addSubscriptionToTree(subscriptionManager, subscription)
      q.push(redis.hset(selector, CACHE, channel + '_tree', newTreeJson))
    }
  } else if (treeVersion) {
    // remove tree ?
  }

  if (currentVersion === newVersion) {
    return
  }

  subscription.version = newVersion

  q.push(
    redis.hmset(
      selector,
      CACHE,
      channel,
      resultStr,
      channel + '_version',
      newVersion
    )
  )

  await Promise.all(q)

  console.log('PUBLISHING', selector, channel, newVersion)
  await redis.publish(selector, channel, newVersion)
}

export default sendUpdate
