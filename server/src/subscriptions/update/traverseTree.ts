import SubscriptionManager from '../subsManager'

const traverse = async (
  subscriptionManager: SubscriptionManager,
  channel: string,
  isDelete: boolean = false
) => {
  // make it batch
  const path = channel.split('.')

  console.log(path)

  const tree = subscriptionManager.tree
}

export default traverse
