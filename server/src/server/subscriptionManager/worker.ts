import { ServerOptions } from '../../types'
import { SubscriptionManager } from './types'
import { SelvaClient, constants } from '@saulx/selva'
import { parentPort } from 'worker_threads'
import addListeners from './addListeners'
import chalk from 'chalk'
import updateSubscriptionData from './updateSubscriptionData'

const { SERVER_HEARTBEAT } = constants

const clear = (subsManager: SubscriptionManager) => {
  subsManager.clients = {}
  subsManager.subscriptions = {}
  subsManager.originListeners = {}
  subsManager.tree = {}
  subsManager.stagedInProgess = false
  subsManager.stagedForUpdates = new Set()
  clearTimeout(subsManager.stagedTimeout)
  clearTimeout(subsManager.revalidateSubscriptionsTimeout)
  clearTimeout(subsManager.serverHeartbeatTimeout)
  clearTimeout(subsManager.refreshNowQueriesTimeout)
}

const startServerHeartbeat = (subsManager: SubscriptionManager) => {
  const setHeartbeat = () => {
    subsManager.client.redis.publish(
      subsManager.selector,
      SERVER_HEARTBEAT,
      String(Date.now())
    )
    subsManager.serverHeartbeatTimeout = setTimeout(setHeartbeat, 2e3)
  }
  setHeartbeat()
}

const revalidateSubscriptions = (subsManager: SubscriptionManager) => {
  updateSubscriptionData(subsManager)
  subsManager.revalidateSubscriptionsTimeout = setTimeout(() => {
    revalidateSubscriptions(subsManager)
  }, 1 * 30e3)
}

const createSubscriptionManager = (
  opts: ServerOptions
): SubscriptionManager => {
  console.log(chalk.white('Start subscriptionManager worker'))

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
    tree: {},
    selector: {
      port: opts.port,
      host: opts.host
    },
    originListeners: {}
  }

  client.redis.registry.on('connect', () => {
    console.log(chalk.white('Connected subscriptionManager selva client'))

    addListeners(subsManager)
    startServerHeartbeat(subsManager)
    updateSubscriptionData(subsManager)
    revalidateSubscriptions(subsManager)

    parentPort.postMessage(
      JSON.stringify({
        event: 'connect'
      })
    )
  })

  client.redis.registry.on('disconnect', () => {
    console.log('disconnect server-client')
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
      console.log('Destroy subs client')
      destroy(subsManager)
    }
  } catch (_err) {}
})
