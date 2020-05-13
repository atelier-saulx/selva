import { SubscriptionManager } from '../types'
import addUpdate from './addUpdate'
import contains from './contains'

const traverse = (
  subscriptionManager: SubscriptionManager,
  channel: string
) => {
  // make it batch
  const path = channel.split('.')
  const id = path[0]

  // we can add this to tree
  if (subscriptionManager.memberMemCache[channel]) {
    console.log('Remove from memcache update!', path)
    delete subscriptionManager.memberMemCache[channel]
    subscriptionManager.memberMemCacheSize--
  }

  let segment = subscriptionManager.tree
  let prefix: string | undefined
  for (let i = 1; i < path.length; i++) {
    segment = segment[path[i]]
    if (segment) {
      if (segment.___ids) {
        const subs = segment.___ids[id]
        if (subs) {
          subs.forEach(subscription => {
            if (!subscription.inProgress) {
              addUpdate(subscriptionManager, subscription)
            }
          })
        }
      }

      if (segment.___types) {
        if (!prefix) {
          prefix = id.slice(0, 2)
        }
        const match = segment.___types[prefix]
        if (match) {
          for (let containsId in match) {
            contains(subscriptionManager, containsId, id, match[containsId])
          }
        }
      }

      if (segment.__any) {
        for (let containsId in segment.__any) {
          contains(
            subscriptionManager,
            containsId,
            id,
            segment.__any[containsId]
          )
        }
      }
    } else {
      break
    }
  }
}

export default traverse
