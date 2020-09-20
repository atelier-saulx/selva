// hack for colors in a worker process
for (const stream of ['stdout', 'stdin', 'stderr']) {
  if (process[stream].isTTY === undefined) {
    process[stream].isTTY = true
  }
}

// import chalk from 'chalk'
// console.log(chalk.red('x'))

import { ServerOptions } from '../../types'
import { SubscriptionManager } from './types'
import { SelvaClient } from '@saulx/selva'
import { parentPort } from 'worker_threads'
import addListeners from './addListeners'
import updateSubscriptionData from './updateSubscriptionData'

const clear = (subsManager: SubscriptionManager) => {
  subsManager.clients = {}
  subsManager.subscriptions = {}
  subsManager.originListeners = {}
  subsManager.tree = {}
  subsManager.stagedInProgess = false
  subsManager.stagedForUpdates = new Set()
  clearTimeout(subsManager.stagedTimeout)
  clearTimeout(subsManager.revalidateSubscriptionsTimeout)
  clearTimeout(subsManager.refreshNowQueriesTimeout)
}

const revalidateSubscriptions = (subsManager: SubscriptionManager) => {
  updateSubscriptionData(subsManager)
  subsManager.revalidateSubscriptionsTimeout = setTimeout(() => {
    revalidateSubscriptions(subsManager)
  }, 1 * 5e3)
}

const createSubscriptionManager = (
  opts: ServerOptions
): SubscriptionManager => {
  const client = new SelvaClient(opts.registry)

  const subsManager: SubscriptionManager = {
    client,
    incomingCount: 0,
    stagedForUpdates: new Set(),
    stagedInProgess: false,
    memberMemCacheSize: 0,
    memberMemCache: {},
    clients: {},
    subscriptions: {},
    inProgressCount: 0,
    tree: {},
    selector: {
      port: opts.port,
      host: opts.host
    },
    originListeners: {}
  }

  client.on('connect', () => {
    addListeners(subsManager)
    updateSubscriptionData(subsManager)
    revalidateSubscriptions(subsManager)

    parentPort.postMessage(
      JSON.stringify({
        event: 'connect'
      })
    )
  })

  client.on('disconnect', () => {
    clear(subsManager)
  })

  return subsManager
}

const destroy = (subsManager: SubscriptionManager) => {
  subsManager.client.destroy()
  clear(subsManager)
  parentPort.postMessage(
    JSON.stringify({
      event: 'destroyComplete'
    })
  )
}

// ---- worker starting ----
let subsManager: SubscriptionManager
parentPort.on('message', (message: string) => {
  try {
    const { event, payload } = JSON.parse(message)
    if (event === 'connect') {
      if (subsManager) {
        destroy(subsManager)
      }
      subsManager = createSubscriptionManager(<ServerOptions>payload)
    } else if (event === 'destroy') {
      destroy(subsManager)
    }
  } catch (_err) {}
})
