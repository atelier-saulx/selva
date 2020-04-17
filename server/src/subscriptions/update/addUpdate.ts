import SubscriptionManager from '../subsManager'
import sendUpdate from './sendUpdate'
import { Subscription } from '../'

var delayCount = 0

const sendUpdates = (subscriptionManager: SubscriptionManager) => {
  console.log(
    'SEND UPDATES - handled events:',
    subscriptionManager.stagedForUpdates.size,
    subscriptionManager.incomingCount
  )

  subscriptionManager.stagedForUpdates.forEach(subscription => {
    subscription.inProgress = false
    console.log('update subscription and clear inProgress', subscription.get)
    subscriptionManager.stagedForUpdates.delete(subscription)
    sendUpdate(subscriptionManager, subscription)
      .then(v => {
        console.log('SEND UPDATE FOR', subscription.channel)
      })
      .catch(err => {
        console.log('WRONG ERROR IN SENDUPDATE', err)
      })
  })

  subscriptionManager.stagedInProgess = false
  subscriptionManager.incomingCount = 0
  delayCount = 0
}

const rate = 3

const delay = (subscriptionManager, time = 1000, totalTime = 0) => {
  if (totalTime < 10e3) {
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
        time *= 1.1
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
      '10 seconds pass drain',
      totalTime,
      'incoming',
      subscriptionManager.incomingCount
    )
    // do it now
    sendUpdates(subscriptionManager)
  }
}

const addUpdate = (
  subscriptionManager: SubscriptionManager,
  subscription: Subscription,
  custom?: { type: string; payload?: any }
) => {
  if (subscription.inProgress) {
    if (!subscriptionManager.stagedInProgess) {
      console.error('CANNOT HAVE BATCH UPDATES IN PROGRESS + SUBS IN PROGRESS')
    }
    console.log('Sub in progess')
  } else {
    if (custom) {
      subscription.inProgress = true
      sendUpdate(subscriptionManager, subscription, custom)
        .then(v => {
          subscription.inProgress = false
        })
        .catch(err => {
          console.error('error in custom sendUpdate', err)
          subscription.inProgress = false
        })
    } else {
      subscriptionManager.stagedForUpdates.add(subscription)
      subscription.inProgress = true
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
