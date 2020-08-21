import sendUpdate from './sendUpdate'
import { Subscription, SubscriptionManager } from '../types'
import { removeSubscriptionFromTree } from '../tree'

var delayCount = 0

const sendUpdates = (subscriptionManager: SubscriptionManager) => {
  // seeing a double subscriptions no good
  let cnt = 0
  subscriptionManager.stagedForUpdates.forEach(subscription => {
    subscription.inProgress = false
    subscriptionManager.stagedForUpdates.delete(subscription)
    if (subscription.beingProcessed) {
      // console.log('in progress dont add')
      subscription.processNext = true
    } else {
      cnt++
      sendUpdate(subscriptionManager, subscription)
        .then(v => {
          // console.log('SEND UPDATE FOR', subscription.channel)
        })
        .catch(err => {
          console.log('WRONG ERROR IN SENDUPDATE', err)
          console.log(
            'ok still beign processed?',
            subscription.beingProcessed,
            subscription.processNext
          )
          subscriptionManager.inProgressCount--
          subscription.beingProcessed = false
          if (subscription.processNext) {
            subscription.processNext = false
          }
        })
    }
  })

  // if (cnt) {
  //   console.log(
  //     'subsManager',
  //     subscriptionManager.client.uuid.slice(-6),
  //     'Handled',
  //     subscriptionManager.incomingCount,
  //     'beingProcessed',
  //     subscriptionManager.inProgressCount,
  //     'actual subscriptions being updated',
  //     cnt
  //   )
  // }

  subscriptionManager.stagedInProgess = false

  subscriptionManager.incomingCount = 0
  subscriptionManager.memberMemCache = {}

  if (subscriptionManager.memberMemCacheSize > 1e5) {
    console.log('memberMemCache is larger then 100k flush')
    subscriptionManager.memberMemCacheSize = 0
  }
  delayCount = 0
}

// 10 per ms
const eventsPerMs = 100

const delay = (subscriptionManager, time = 1000, totalTime = 0) => {
  if (totalTime < 10e3) {
    const lastIncoming = subscriptionManager.incomingCount

    delayCount++
    console.log('Sendupdate delay #', delayCount, lastIncoming)
    subscriptionManager.stagedTimeout = setTimeout(() => {
      const incoming = subscriptionManager.incomingCount - lastIncoming
      if (incoming / time > eventsPerMs) {
        time = Math.round(time * 1.1)
        subscriptionManager.stagedTimeout = setTimeout(() => {
          delay(subscriptionManager, time, totalTime + time)
        }, time)
      } else {
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
    sendUpdates(subscriptionManager)
  }
}

const addUpdate = (
  subscriptionManager: SubscriptionManager,
  subscription: Subscription
) => {
  if (subscription.inProgress) {
    if (!subscriptionManager.stagedInProgess) {
      console.error('CANNOT HAVE BATCH UPDATES IN PROGRESS + SUBS IN PROGRESS')
    }
  } else {
    subscriptionManager.stagedForUpdates.add(subscription)
    subscription.inProgress = true
    if (!subscriptionManager.stagedInProgess) {
      subscriptionManager.stagedInProgess = true
      subscriptionManager.stagedTimeout = setTimeout(() => {
        const { incomingCount } = subscriptionManager
        if (incomingCount < 1e4) {
          sendUpdates(subscriptionManager)
        } else {
          delay(subscriptionManager)
        }
      }, 10)
    }
  }
}

export default addUpdate
