import { prefixes } from '@saulx/selva'
import SubscriptionManager from '../subsManager'
import { addSubscriptionToTree, removeSubscriptionFromTree } from '../tree'
import { hash } from '../util'
import { Subscription } from '../'

const sendUpdate = async (
  subscriptionManager: SubscriptionManager,
  subscription: Subscription
) => {
  const channel = subscription.channel

  const getOptions = subscription.get
  getOptions.$includeMeta = true
  const payload = await subscriptionManager.client.get(getOptions)

  // call $meta tree
  const newTree = payload.$meta

  delete payload.$meta

  const resultStr = JSON.stringify({ type: 'update', payload })
  const currentVersion = subscription.version
  const newVersion = hash(resultStr)

  const newTreeJson = JSON.stringify(newTree)
  const newTreeVersion = hash(newTreeJson)
  const treeVersion = subscription.treeVersion

  const q = []

  if (treeVersion !== newTreeVersion) {
    if (treeVersion) {
      removeSubscriptionFromTree(subscriptionManager, subscription)
    }
    subscription.treeVersion = newTreeVersion
    subscription.tree = newTree
    addSubscriptionToTree(subscriptionManager, subscription)
    q.push(
      subscriptionManager.client.redis.byType.hset(
        'sClient',
        prefixes.cache,
        channel + '_tree',
        newTreeJson
      )
    )
  }

  if (currentVersion === newVersion) {
    return
  }

  subscription.version = newVersion

  q.push(
    subscriptionManager.client.redis.byType.hmset(
      'sClient',
      prefixes.cache,
      channel,
      resultStr,
      channel + '_version',
      newVersion
    )
  )

  await Promise.all(q)

  await subscriptionManager.client.redis.byType.publish(
    'sClient',
    channel,
    newVersion
  )
}

export default sendUpdate
