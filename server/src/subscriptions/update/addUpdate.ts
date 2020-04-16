import SubscriptionManager from '../subsManager'
import sendUpdate from './sendUpdate'
/*
  if (!subscription) {
    console.error(`Cannot find subscription on server ${channel.slice(-5)}`)
    return
  }

*/

// const addToUpdateQueue = (subsManager: SubscriptionManager, key: string) => {
//   if (!subsManager.inProgress[key]) {
//     subsManager.inProgress[key] = true
//     batchUpdates.push(key)
//     if (!inProgress) {
//       inProgress = true
//       // want to manage the time update based on amount of things
//       setTimeout(
//         () => {
//           // maybe check amount and slowly drain
//           console.log('QUERIES TO EXEC', batchUpdates.length)
//           // check size for drainage
//           for (let i = 0; i < batchUpdates.length; i++) {
//             const key = batchUpdates[i]
//             subsManager.sendUpdate(key).catch(err => {
//               console.error(`Error query update from subscription ${key}`, err)
//             })
//           }
//           batchUpdates = []
//           inProgress = false
//           subsManager.cleanUpProgress()
//           memberMemCache = {}
//         },
//         subsManager.incomingCount > 15000
//           ? 1500
//           : subsManager.incomingCount > 1000
//           ? 1000
//           : subsManager.incomingCount > 500
//           ? 500
//           : 100
//       )
//     }
//   }
// }

const addUpdate = async (
  subscriptionManager: SubscriptionManager,
  channel: string,
  isDelete: boolean = false
) => {
  const subscription = subscriptionManager.subscriptions[channel]

  if (!subscription) {
    // this should never happen
    console.error(`Cannot find subscription on server ${channel}`)
    return
  }

  if (subscription.inProgress) {
    console.log('Sub in progess')
  } else {
    // handle batch mechanism

    await sendUpdate(subscriptionManager, channel, isDelete)
    subscription.inProgress = false
  }
}

export default addUpdate
