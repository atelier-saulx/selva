import SubscriptionManager from './index'
import query from './query'

const attach = async (
  subsManager: SubscriptionManager,
  port: number
): Promise<void> => {
  subsManager.sub.on('error', () => {})
  subsManager.pub.on('error', () => {})

  let tm: NodeJS.Timeout
  try {
    await Promise.race([
      new Promise((resolve, _reject) => {
        let count = 0
        subsManager.pub.on('ready', () => {
          count++
          if (count === 2) {
            if (tm) {
              clearTimeout(tm)
            }
            resolve()
          }
        })

        subsManager.sub.on('ready', () => {
          count++
          if (count === 2) {
            if (tm) {
              clearTimeout(tm)
            }

            resolve()
          }
        })
      }),
      new Promise((_resolve, reject) => {
        tm = setTimeout(() => {
          subsManager.pub.removeAllListeners('ready')
          subsManager.sub.removeAllListeners('ready')
          reject()
        }, 5000)
      })
    ])
  } catch (e) {
    setTimeout(() => {
      subsManager.attach(port)
    }, 1000)
  }

  await subsManager.refreshSubscriptions()
  subsManager.recalculateUpdates()

  // make it better

  let clearTm: NodeJS.Timeout | undefined

  // client heartbeat events
  subsManager.sub.on('message', (_channel, message) => {
    const payload: { channel: string; refresh?: boolean } = JSON.parse(message)

    const subId = payload.channel.slice('___selva_subscription:'.length)
    subsManager.lastHeartbeat[subId] = Date.now()

    if (payload.refresh) {
      delete subsManager.lastResultHash[subId]
      subsManager
        .refreshSubscription(subId)
        .then(() => {
          return subsManager.sendUpdate(subId)
        })
        .catch(e => {
          console.error(e)
        })
    }
  })

  // lua object change events

  const prefixLength = '___selva_events:'.length

  subsManager.sub.on('pmessage', (_pattern, channel, message) => {
    subsManager.incomingCount++

    if (!clearTm) {
      clearTm = setTimeout(() => {
        clearTm = undefined
        subsManager.incomingCount = 0
      }, 1e3)
    }

    subsManager.lastModifyEvent = Date.now()
    if (channel === '___selva_events:heartbeat') {
      return
    }

    // used to deduplicate events for subscriptions,
    // firing only once if multiple fields in subscription are changed
    const updatedSubscriptions: Record<string, true> = {}

    const eventName = channel.slice(prefixLength)

    if (message === 'delete') {
      for (const field in subsManager.subscriptionsByField) {
        if (field.startsWith(eventName)) {
          const subscriptionIds: Set<string> | undefined =
            subsManager.subscriptionsByField[field] || new Set()
          for (const subscriptionId of subscriptionIds) {
            if (updatedSubscriptions[subscriptionId]) {
              continue
            }
            updatedSubscriptions[subscriptionId] = true
            subsManager.sendUpdate(subscriptionId, null, true)
          }
        }
      }
      return
    } else if (message === 'update') {
      const parts = eventName.split('.')
      let field = parts[0]
      for (let i = 0; i < parts.length; i++) {
        const subscriptionIds: Set<string> | undefined =
          subsManager.subscriptionsByField[field] || new Set()
        for (const subscriptionId of subscriptionIds) {
          if (updatedSubscriptions[subscriptionId]) {
            continue
          }
          updatedSubscriptions[subscriptionId] = true
          subsManager.sendUpdate(subscriptionId).catch(e => {
            console.error(e)
          })
        }
        if (i < parts.length - 1) {
          field += '.' + parts[i + 1]
        }
      }
    }

    query(subsManager, message, eventName)
  })

  subsManager.sub.psubscribe('___selva_events:*')
  subsManager.sub.subscribe('___selva_subscription:client_heartbeats')

  const timeout = () => {
    if (Date.now() - subsManager.lastModifyEvent > 1000 * 30) {
      subsManager.detach()
      subsManager.attach(port).catch(e => {
        console.error(e)
      })

      return
    }

    subsManager.heartbeats()

    subsManager
      .refreshSubscriptions()
      .catch(e => {
        console.error(e)
      })
      .finally(() => {
        subsManager.refreshSubscriptionsTimeout = setTimeout(timeout, 1000 * 10)
      })
  }
  timeout()
}

export default attach
