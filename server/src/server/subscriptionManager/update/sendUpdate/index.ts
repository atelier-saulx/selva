import { constants } from '@saulx/selva'
import { hashObjectIgnoreKeyOrder, hash } from '@saulx/utils'
import { Subscription, SubscriptionManager } from '../../types'
import { wait } from '../../../../util'
import diff from '@saulx/selva-diff'
import { gzip as zipCb } from 'zlib'
import { promisify } from 'util'
import chalk from 'chalk'
import { removeRefreshMeta, addRefreshMeta } from '../../updateRefresh'
const gzip = promisify(zipCb)

const { CACHE } = constants

const inProgressTriggers = new Set()

const sendUpdate = async (
  subscriptionManager: SubscriptionManager,
  subscription: Subscription,
  nodeId?: string
) => {
  // if (!subscription) {
  //   return
  // }

  const channel = subscription.channel
  const { client, selector } = subscriptionManager
  const redis = client.redis

  if (subscriptionManager.subscriptions[channel] !== subscription) {
    subscription.beingProcessed = false
    return
  }

  subscriptionManager.inProgressCount++
  subscription.beingProcessed = true
  const getOptions = Object.assign({}, subscription.get)
  getOptions.$includeMeta = true
  getOptions.$subscription = subscription.channel
  getOptions.$originDescriptors = subscription.originDescriptors

  if (nodeId) {
    if (inProgressTriggers.has(subscription.channel + ':' + nodeId)) {
      return
    }

    inProgressTriggers.add(subscription.channel + ':' + nodeId)
    getOptions.$id = nodeId
  }

  const startTime = Date.now()

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

    await redis.publish(selector, channel, JSON.stringify([version]))
    subscription.beingProcessed = false
    return
  }

  const time = setTimeout(() => {
    console.error(chalk.red('Time out (took longer then 15s)' + channel))
    console.dir(getOptions, { depth: 10 })
  }, 15e3)

  let payload
  try {
    payload = await client.get(getOptions)

    const t = Date.now() - startTime

    if (t > 300) {
      console.log('\n----------------------------------------------------')
      console.log('Get subscription took', t, 'ms')
      console.dir(getOptions, { depth: 2 })
      console.log('----------------------------------------------------')
    }
  } catch (err) {
    payload = {
      ___$error___: err.message,
    }
  }

  if (payload.$ignore === true) {
    clearTimeout(time)
    subscriptionManager.inProgressCount--
    inProgressTriggers.delete(subscription.channel + ':' + nodeId)
    subscription.beingProcessed = false
    return
  }

  const newMeta = payload.$meta

  delete payload.$meta

  const newVersion = hashObjectIgnoreKeyOrder(payload)

  const resultStr = JSON.stringify({ type: 'update', payload })

  const currentVersion = subscription.version
  const q = []

  // if sub is removed
  if (
    subscriptionManager.subscriptions[subscription.channel] !== subscription
  ) {
    clearTimeout(time)
    subscriptionManager.inProgressCount--
    inProgressTriggers.delete(subscription.channel + ':' + nodeId)
    subscription.beingProcessed = false
    return
  }

  if (newMeta) {
    const newTreeJson = JSON.stringify(newMeta)
    if (
      newMeta.___refreshAt !==
      (subscription.tree && subscription.tree.___refreshAt)
    ) {
      if (subscription.tree && subscription.tree.refreshAt) {
        removeRefreshMeta(subscriptionManager, subscription)
      }

      subscription.tree = newMeta
      addRefreshMeta(subscriptionManager, subscription)
      q.push(redis.hset(selector, CACHE, channel + '_tree', newTreeJson))
    }
  }

  if (currentVersion === newVersion) {
    clearTimeout(time)
    subscriptionManager.inProgressCount--
    inProgressTriggers.delete(subscription.channel + ':' + nodeId)
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

  let patch

  // maybe add 'expirimental diffs enabled or something'
  if (currentVersion) {
    const prev = JSON.parse(await redis.hget(selector, CACHE, channel))
    // maybe gzip the patch (very efficient format for gzip)
    const diffPatch = diff(prev.payload, payload)

    // gzip only makes sense for a certain size of update
    // patch = (
    //   await (<Promise<Buffer>>gzip(JSON.stringify([diffPatch, currentVersion])))
    // ).toString('base64')
    // console.log('PATCH', patch)

    patch = JSON.stringify([diffPatch, currentVersion])
  }

  if (patch) {
    q.push(
      redis.hmset(
        selector,
        CACHE,
        channel,
        resultStr,
        channel + '_version',
        newVersion,
        channel + '_diff',
        patch
      )
    )
  } else {
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
  }

  await Promise.all(q)

  await redis.publish(
    selector,
    channel,
    JSON.stringify(currentVersion ? [newVersion, currentVersion] : [newVersion])
  )

  clearTimeout(time)

  subscriptionManager.inProgressCount--
  inProgressTriggers.delete(subscription.channel + ':' + nodeId)
  if (subscription.processNext) {
    await wait(100)
    subscription.processNext = false
    await sendUpdate(subscriptionManager, subscription)
  } else {
    subscription.beingProcessed = false
  }
}

export default sendUpdate
