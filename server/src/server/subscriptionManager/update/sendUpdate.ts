import { constants } from '@saulx/selva'
import { addSubscriptionToTree, removeSubscriptionFromTree } from '../tree'
import { hash } from '../util'
import { Subscription, SubscriptionManager } from '../types'
import { wait } from '../../../util'

const { CACHE } = constants

const sendUpdate = async (
  subscriptionManager: SubscriptionManager,
  subscription: Subscription
) => {
  const channel = subscription.channel
  const { client, selector } = subscriptionManager
  const redis = client.redis

  if (subscriptionManager.subscriptions[channel] !== subscription) {
    return
  }

  subscriptionManager.inProgressCount++
  subscription.beingProcessed = true
  const getOptions = subscription.get
  getOptions.$includeMeta = true

  if (channel.startsWith(constants.SCHEMA_SUBSCRIPTION)) {
    const dbName = channel.slice(constants.SCHEMA_SUBSCRIPTION.length + 1)
    const schemaResp = await client.getSchema(dbName)
    const version = schemaResp.schema.sha
    const value = JSON.stringify({ type: 'update', payload: schemaResp.schema })
    await redis.hmset(
      selector,
      CACHE,
      channel,
      value,
      channel + '_version',
      version
    )
    await redis.publish(selector, channel, version)
    subscription.beingProcessed = false
    return
  }

  const time = setTimeout(() => {
    throw new Error('Time out ' + channel)
  }, 15e3)

  let payload
  try {
    payload = await client.get(getOptions)
  } catch (err) {
    payload = {
      ___$error___: err
    }
  }

  const newTree = payload.$meta

  delete payload.$meta

  // make this without payload
  const resultStr = JSON.stringify({ type: 'update', payload })
  const currentVersion = subscription.version
  const newVersion = hash(resultStr)

  const treeVersion = subscription.treeVersion
  const q = []

  // if sub is removed
  if (
    subscriptionManager.subscriptions[subscription.channel] !== subscription
  ) {
    clearTimeout(time)
    subscriptionManager.inProgressCount--
    subscription.beingProcessed = false
    return
  }

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
    clearTimeout(time)
    subscriptionManager.inProgressCount--
    if (subscription.processNext) {
      await wait(100)
      subscription.processNext = false
      await sendUpdate(subscriptionManager, subscription)
    } else {
      subscription.beingProcessed = false
    }
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

  await redis.publish(selector, channel, newVersion)

  clearTimeout(time)

  subscriptionManager.inProgressCount--
  if (subscription.processNext) {
    await wait(100)
    subscription.processNext = false
    await sendUpdate(subscriptionManager, subscription)
  } else {
    subscription.beingProcessed = false
  }
}

export default sendUpdate
