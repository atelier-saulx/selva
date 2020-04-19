import { SelvaClient, GetOptions, ConnectOptions, prefixes } from '@saulx/selva'
import { addSubscriptionToTree, removeSubscriptionFromTree } from './tree'
import addListeners from './addListeners'
import addListenersUpdate from './update/addListeners'
import addUpdate from './update/addUpdate'
import { hash } from './util'
// for add listeners...
import { Worker } from 'worker_threads'

import { Subscription, Tree, RefreshSubscriptions } from './'

export default class SubscriptionManager {
  public client: SelvaClient

  public incomingCount: number = 0

  public stagedForUpdates: Set<Subscription> = new Set()

  public stagedInProgess: boolean = false

  public stagedTimeout: NodeJS.Timeout

  public memberMemCache: Record<string, Record<string, true>> = {}
  // public isDestroyed: boolean = false
  // to check if the server is still ok
  public serverHeartbeatTimeout: NodeJS.Timeout

  public refreshNowQueriesTimeout: NodeJS.Timeout
  // revalidates subs ones in a while
  public revalidateSubscriptionsTimeout: NodeJS.Timeout

  public refreshSubscriptions: RefreshSubscriptions

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

      if (!tree) {
        addUpdate(this, subscription)
      } else {
        this.subscriptions[channel].version = version
        this.subscriptions[channel].tree = JSON.parse(tree)
        this.subscriptions[channel].treeVersion = hash(tree)
        addSubscriptionToTree(this, subscription)
      }
    } else {
      addUpdate(this, subscription)
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
    let [subscriptions, clients] = await Promise.all([
      byType.hgetall('sClient', prefixes.subscriptions),
      byType.hgetall('sClient', prefixes.clients)
    ])

    if (!subscriptions) {
      subscriptions = {}
    }

    if (!clients) {
      clients = {}
    }

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
    }, 1 * 30e3)
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
    this.clients = {}
    // for each sub remove refreshAt

    this.subscriptions = {}
    this.tree = {}

    this.stagedInProgess = false
    this.stagedForUpdates = new Set()
    clearTimeout(this.stagedTimeout)

    clearTimeout(this.revalidateSubscriptionsTimeout)
    clearTimeout(this.serverHeartbeatTimeout)
    clearTimeout(this.refreshNowQueriesTimeout)
  }

  async connect(opts: ConnectOptions): Promise<void> {
    let isResolved = false

    console.log('ConnectOptions SERVER', opts)

    return new Promise(resolve => {
      // make promise assignable as well
      // { loglevel: 'info' }
      this.client = new SelvaClient(opts)

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
}
