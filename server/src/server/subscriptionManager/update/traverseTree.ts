import { SubscriptionManager } from '../types'
import addUpdate from './addUpdate'
import contains from './contains'

const traverse = (
  subscriptionManager: SubscriptionManager,
  channel: string,
  dbName: string
) => {
  // make it batch
  const path = channel.split('.')
  const id = path[0]

  // we can add this to tree
  if (
    subscriptionManager.memberMemCache[dbName] &&
    subscriptionManager.memberMemCache[dbName][channel]
  ) {
    console.log('Remove from memcache update!', path)
    delete subscriptionManager.memberMemCache[dbName][channel]
    subscriptionManager.memberMemCacheSize--
  }

  let segment = subscriptionManager.tree[dbName]
  if (!segment) {
    return
  }

  let prefix: string | undefined
  for (let i = 1; i < path.length; i++) {
    segment = segment[path[i]]
    if (segment) {
      if (segment.___ids) {
        const subs = segment.___ids[id]

        if (subs) {
          // @ts-ignore
          if (global.log) {
            console.log('got subs', id)
          }
          subs.forEach(subscription => {
            if (!subscription.inProgress) {
              // @ts-ignore
              if (global.log) {
                if (
                  subscription.channel ===
                  '1d916a69704f2c08841a1630a0166ec4eb75decad388f2f4f706f45dc514b4bc'
                ) {
                  // this id has to be in meta
                  console.log('GO UPDATE FOUND CORRECT', id)
                }
              }

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
            contains(
              subscriptionManager,
              containsId,
              { id, db: dbName },
              match[containsId]
            )
          }
        }
      }

      if (segment.__any) {
        // @ts-ignore
        if (global.log) {
          // if (
          //   subscription.channel ===
          //   '1d916a69704f2c08841a1630a0166ec4eb75decad388f2f4f706f45dc514b4bc'
          // ) {
          console.log('GO UPDATE', id)
          // }
        }

        for (let containsId in segment.__any) {
          contains(
            subscriptionManager,
            containsId,
            { id, db: dbName },
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
