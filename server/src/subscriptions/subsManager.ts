import { SelvaClient, GetOptions, ConnectOptions, prefixes } from '@saulx/selva'
import { addSubscriptionToTree, removeSubscriptionFromTree } from './tree'
import addListeners from './addListeners'
import addListenersUpdate from './update/addListeners'
import addUpdate from './update/addUpdate'
import { hash } from './util'

// for add listeners...
import { Worker } from 'worker_threads'

import { Subscription, Tree } from '.'

export default class SubscriptionManager {
  public client: SelvaClient

  public inProgress: Record<string, true> = {}
  public incomingCount: number = 0
  // public isDestroyed: boolean = false
  public cleanUp: boolean = false
  // to check if the server is still ok
  public serverHeartbeatTimeout: NodeJS.Timeout
  // revalidates subs ones in a while
  public revalidateSubscriptionsTimeout: NodeJS.Timeout

  public clients: Record<
    string,
    { lastTs: number; subscriptions: Set<string> }
  > = {}

  public subscriptions: Record<string, Subscription> = {}

  public tree: Tree = {}

  async addClientSubscription(client: string, channel: string) {
    // just to speed things up and potentialy send something
    if (!this.subscriptions[channel]) {
      const [getOptions, clients] = await Promise.all([
        this.client.redis.byType.hget(
          'sClient',
          prefixes.subscriptions,
          channel
        ),
        this.client.redis.byType.smembers('sClient', channel)
      ])

      if (getOptions && clients.length) {
        this.addSubscription(channel, new Set(clients), JSON.parse(getOptions))
      }
    } else {
      this.subscriptions[channel].clients.add(client)
    }
  }

  async removeClientSubscription(client: string, channel: string) {
    // just to speed things up
    // use client redis internally for everything - much better
    const { byType } = this.client.redis
    const clients = await byType.smembers('sClient', channel)
    const sub = this.subscriptions[channel]
    const cleanUpQ = []
    let len = clients.length
    if (clients.indexOf(client) !== -1) {
      len--
      cleanUpQ.push(byType.srem('sClient', channel, client))
    }
    if (sub) {
      sub.clients.delete(client)
    }
    if (len === 0) {
      this.removeSubscription(channel, cleanUpQ)
      delete this.subscriptions[channel]
    }
    if (cleanUpQ.length) {
      await Promise.all(cleanUpQ)
    }
  }

  async addSubscription(
    channel: string,
    clients: Set<string>,
    getOptions: GetOptions
  ) {
    const { byType } = this.client.redis
    const subscription = {
      clients,
      channel,
      get: getOptions
    }
    this.subscriptions[channel] = subscription
    if (await byType.hexists('sClient', prefixes.cache, channel)) {
      const [tree, version] = await byType.hmget(
        'sClient',
        prefixes.cache,
        channel + '_tree',
        channel + '_version'
      )
      this.subscriptions[channel].version = version
      this.subscriptions[channel].tree = JSON.parse(tree)
      this.subscriptions[channel].treeVersion = hash(tree)
      addSubscriptionToTree(this, subscription)
    } else {
      await addUpdate(this, subscription)
    }
  }

  async removeSubscription(channel: string, cleanUpQ: any[] = []) {
    const { byType } = this.client.redis
    const { subscriptions } = this
    cleanUpQ.push(byType.hdel('sClient', prefixes.subscriptions, channel))
    cleanUpQ.push(byType.del('sClient', channel))
    cleanUpQ.push(
      byType.hdel(
        'sClient',
        prefixes.cache,
        channel,
        channel + '_version',
        channel + '_tree'
      )
    )
    if (channel in subscriptions) {
      removeSubscriptionFromTree(this, subscriptions[channel])
      delete subscriptions[channel]
    }
  }

  async updateSubscriptionData() {
    const { byType } = this.client.redis
    const [subscriptions, clients] = await Promise.all([
      byType.hgetall('sClient', prefixes.subscriptions),
      byType.hgetall('sClient', prefixes.clients)
    ])
    const now = Date.now()
    const cleanUpQ = []

    for (const client in clients) {
      const ts = Number(clients[client])
      if (now - ts < 60e3) {
        // client is not timed out
        if (client in this.clients) {
          this.clients[client].lastTs = ts
        } else {
          // no client add it
          this.clients[client] = {
            subscriptions: new Set(),
            lastTs: ts
          }
        }
      } else {
        console.log('Client is timedout from server', client)
        cleanUpQ.push(byType.hdel('sClient', prefixes.clients, client))
        if (client in this.clients) {
          // need to remove client
          delete this.clients[client]
        }
      }
    }

    await Promise.all(
      Object.keys(subscriptions).map(async channel => {
        const subscriptionClients = await byType.smembers('sClient', channel)
        if (channel in this.subscriptions) {
          for (let i = subscriptionClients.length - 1; i >= 0; i--) {
            const client = subscriptionClients[i]
            if (!this.clients[client]) {
              subscriptionClients.splice(i, 1)
              cleanUpQ.push(byType.srem('sClient', channel, client))
            }
          }

          if (this.subscriptions[channel].clients.size === 0) {
            this.removeSubscription(channel, cleanUpQ)
          }
        } else {
          const clientsSet: Set<string> = new Set()
          for (let i = subscriptionClients.length - 1; i >= 0; i--) {
            const client = subscriptionClients[i]
            if (client in this.clients) {
              clientsSet.add(client)
            } else {
              subscriptionClients.splice(i, 1)
              cleanUpQ.push(byType.srem('sClient', channel, client))
            }
          }
          if (clientsSet.size) {
            this.addSubscription(
              channel,
              clientsSet,
              <GetOptions>JSON.parse(subscriptions[channel])
            )
          }
        }
      })
    )

    if (cleanUpQ.length) {
      await Promise.all(cleanUpQ)
      console.log('cleaned up clients / subscriptions', cleanUpQ.length)
    }
  }

  revalidateSubscriptions() {
    // maybe delay
    this.updateSubscriptionData()
    this.revalidateSubscriptionsTimeout = setTimeout(() => {
      this.revalidateSubscriptions()
    }, 1 * 60e3)
  }

  startServerHeartbeat() {
    const setHeartbeat = () => {
      const { sClient } = this.client.redis.redis
      if (sClient) {
        sClient.publish(prefixes.serverHeartbeat, String(Date.now()))
      }
      this.serverHeartbeatTimeout = setTimeout(setHeartbeat, 2e3)
    }
    setHeartbeat()
  }

  clear() {
    this.inProgress = {}
    this.clients = {}
    this.subscriptions = {}
    this.tree = {}
    this.cleanUp = false
    clearTimeout(this.revalidateSubscriptionsTimeout)
    clearTimeout(this.serverHeartbeatTimeout)
  }

  async connect(opts: ConnectOptions): Promise<void> {
    let isResolved = false

    console.log('ConnectOptions SERVER', opts)

    return new Promise(resolve => {
      // make promise assignable as well
      this.client = new SelvaClient(opts, { loglevel: 'info' })

      this.client.on('connect', () => {
        console.log('connect server-client')

        addListeners(this)
        addListenersUpdate(this)

        this.startServerHeartbeat()
        this.updateSubscriptionData()

        // this.client.redis.hdel(prefixes.cache)
        this.revalidateSubscriptions()
        if (!isResolved) {
          isResolved = true
          resolve()
        }
      })

      this.client.on('disconnect', () => {
        console.log('disconnect server-client')
        this.clear()
      })
    })
  }

  destroy() {
    this.client.destroy()
    this.clear()
  }

  cleanUpProgress() {
    if (!this.cleanUp) {
      this.cleanUp = true
      setTimeout(() => {
        this.inProgress = {}
        this.cleanUp = false
      }, 250)
    }
  }
}
