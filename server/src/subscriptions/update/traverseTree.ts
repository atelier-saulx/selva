import SubscriptionManager from '../subsManager'
import addUpdate from './addUpdate'

const traverse = async (
  subscriptionManager: SubscriptionManager,
  channel: string,
  isDelete: boolean = false
) => {
  // make it batch
  const path = channel.split('.')
  const id = path[0]

  let segment = subscriptionManager.tree
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

      if (segment.__type) {
      }

      if (segment.__any) {
      }
    } else {
      break
    }
  }
}

export default traverse
