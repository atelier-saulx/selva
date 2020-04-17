import SubscriptionManager from '../subsManager'
import sendUpdate from './sendUpdate'
import { Subscription } from '../'

var delayCount = 0

const sendUpdates = (subscriptionManager: SubscriptionManager) => {
  console.log(
    'SEND UPDATES - handled events:',
    subscriptionManager.incomingCount
  )

  subscriptionManager.stagedForUpdates.forEach(subscription => {
    subscription.inProgress = false

    console.log('try updating subscription', subscription.channel)
    sendUpdate(subscriptionManager, subscription)
      .then(v => {
        console.log('DID SEND UPDATE FOR', subscription.channel)
      })
      .catch(err => {
        console.log('WRONG ERROR IN SENDUPDATE', err)
      })
  })

  console.log('yesh done updating')
  subscriptionManager.stagedForUpdates = new Set()
  subscriptionManager.stagedInProgess = false
  subscriptionManager.incomingCount = 0
  delayCount = 0
}

const rate = 5

const delay = (subscriptionManager, time = 1000, totalTime = 0) => {
  if (totalTime < 20e3) {
    const lastIncoming = subscriptionManager.incomingCount
    delayCount++
    console.log('delay #', delayCount, lastIncoming)
    subscriptionManager.stagedTimeout = setTimeout(() => {
      const incoming = subscriptionManager.incomingCount - lastIncoming
      if (incoming / time > rate) {
        // too fast ait a bit longer
        // reset count
        // subscriptionManager.incomingCount = 0
        // increase time
        time *= 1.5

        // delay again
        subscriptionManager.stagedTimeout = setTimeout(() => {
          delay(subscriptionManager, time, totalTime + time)
        }, time)
      } else {
        // do it
        sendUpdates(subscriptionManager)
      }
    }, time)
  } else {
    console.log(
      '20 seconds pass drain',
      totalTime,
      'incoming',
      subscriptionManager.incomingCount
    )
    // do it now
    sendUpdates(subscriptionManager)
  }
}

const addUpdate = async (
  subscriptionManager: SubscriptionManager,
  subscription: Subscription,
  custom?: { type: string; payload?: any }
) => {
  if (subscription.inProgress) {
    console.log('Sub in progess')
  } else {
    // handle batch mechanism
    subscription.inProgress = true

    if (custom) {
      await sendUpdate(subscriptionManager, subscription, custom)
      subscription.inProgress = false
    } else {
      subscriptionManager.stagedForUpdates.add(subscription)

      if (!subscriptionManager.stagedInProgess) {
        subscriptionManager.stagedInProgess = true
        subscriptionManager.stagedTimeout = setTimeout(() => {
          const { incomingCount } = subscriptionManager
          if (incomingCount < 1000) {
            sendUpdates(subscriptionManager)
          } else {
            delay(subscriptionManager)
          }
        }, 100)
      }
    }
  }
}

export default addUpdate
