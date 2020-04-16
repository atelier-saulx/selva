import { RedisClient } from 'redis'
import { SelvaClient, GetOptions, ConnectOptions, prefixes } from '@saulx/selva'
import {
  addFieldsToSubscription,
  removeFieldsFromSubscription
} from './addFields'
import addListeners from './addListeners'
import sendUpdate from './sendUpdate'

// for add listeners...
import {
  Worker,
  isMainThread,
  parentPort,
  workerData,
  MessageChannel
} from 'worker_threads'

import { QuerySubscription, Subscription, RefsById, Fields } from '.'

export default class SubscriptionManager {
  public queries: Record<string, QuerySubscription[]> = {}
  public nowBasedQueries: {
    nextRefresh: number
    queries: { subId: string; nextRefresh: number }[]
  }
  public refsById: RefsById = {}

  public eventHandlerWorker: Worker

  public client: SelvaClient
  public sub: RedisClient
  public pub: RedisClient // pub and client
  public sSub: RedisClient
  public sPub: RedisClient // pub and client

  public inProgress: Record<string, true> = {}
  public incomingCount: number = 0
  // public isDestroyed: boolean = false
  public cleanUp: boolean = false
  public refreshNowQueriesTimeout: NodeJS.Timeout
  // to check if the server is still ok
  public serverHeartbeatTimeout: NodeJS.Timeout
  // revalidates subs ones in a while
  public revalidateSubscriptionsTimeout: NodeJS.Timeout

  public clients: Record<
    string,
    { lastTs: number; subscriptions: Set<string> }
  > = {}
  public subscriptions: Record<string, Subscription> = {}
  public fieldMap: Fields = {}

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

    // for now can send a ping to a client channel where we can publish the initial
    // normally the client just gets it
  }

  async removeClientSubscription(client: string, channel: string) {
    // just to speed things up
    // use client redis internally for everything - much better
    const cleanUpQ = []
    const clients = await this.client.redis.byType.smembers('sClient', channel)
    let len = clients.length
    const cIndex = clients.indexOf(client)
    if (cIndex !== -1) {
      len--
      cleanUpQ.push(this.client.redis.byType.srem('sClient', channel, client))
    }
    const sub = this.subscriptions[channel]
    if (sub) {
      sub.clients.delete(client)
    }
    if (len === 0) {
      this.removeSubscription(channel, cleanUpQ)
      delete this.subscriptions[channel]
    }
    if (cleanUpQ.length) {
      await Promise.all(cleanUpQ)
      // console.log(
      //   'cleaned up subscriptions from removeClientSubscription',
      //   cleanUpQ.length
      // )
    }
  }

  async addSubscription(
    channel: string,
    clients: Set<string>,
    getOptions: GetOptions
  ) {
    this.subscriptions[channel] = {
      clients,
      get: getOptions,
      fields: new Set()
    }

    // FIXME: use schema subs!
    if (!this.client.schema) {
      await this.client.getSchema()
    }

    addFieldsToSubscription(
      this.subscriptions[channel],
      this.fieldMap,
      // FIXME: schema needs an observer!
      this.client.schema,
      channel,
      this.refsById
    )

    // have to check what the last update was ESPECIALY WHEN
    if (!(await this.client.redis.hexists(prefixes.cache, channel))) {
      await this.sendUpdate(channel)
    }
  }

  async removeSubscription(channel: string, cleanUpQ: any[] = []) {
    cleanUpQ.push(
      this.client.redis.byType.hdel('sClient', prefixes.subscriptions, channel)
    )
    cleanUpQ.push(this.client.redis.byType.del('sClient', channel))
    cleanUpQ.push(
      this.client.redis.byType.hdel(
        'sClient',
        prefixes.cache,
        channel,
        channel + '_version'
      )
    )

    if (this.queries[channel]) {
      delete this.queries[channel]
    }

    if (this.subscriptions[channel]) {
      removeFieldsFromSubscription(
        this.subscriptions[channel],
        this.fieldMap,
        channel,
        this.refsById
      )
      delete this.subscriptions[channel]
    }
  }

  async updateSubscriptionData() {
    // in progress as well
    // has to become lua
    // can we do less here maybe?
    // e.g do a diff first or something
    // can do multi if you want
    const [subscriptions, clients] = await Promise.all([
      this.client.redis.byType.hgetall('sClient', prefixes.subscriptions),
      this.client.redis.byType.hgetall('sClient', prefixes.clients)
    ])

    const q = []
    for (const channel in subscriptions) {
      q.push(this.client.redis.byType.smembers('sClient', channel))
    }

    const subsClients = await Promise.all(q)

    this.clients = {}
    this.subscriptions = {}
    this.fieldMap = {}
    this.refsById = {}

    const cleanUpQ = []

    const now = Date.now()
    for (const client in clients) {
      const ts = Number(clients[client])
      if (now - ts < 60e3) {
        this.clients[client] = {
          lastTs: ts,
          subscriptions: new Set()
        }
      } else {
        console.log('Client is timedout from server', client)
        cleanUpQ.push(
          this.client.redis.byType.hdel('sClient', prefixes.clients, client)
        )
        // publish too client that its marked as timeout
      }
    }

    let i = 0
    for (const channel in subscriptions) {
      const cl = subsClients[i]
      if (cl.length) {
        const clientsSet: Set<string> = new Set()
        for (let i = 0; i < cl.length; i++) {
          const client = cl[i]
          if (client in this.clients) {
            clientsSet.add(client)
          } else {
            cleanUpQ.push(
              this.client.redis.byType.srem('sClient', channel, client)
            )
          }
        }
        if (clientsSet.size) {
          this.addSubscription(
            channel,
            clientsSet,
            <GetOptions>JSON.parse(subscriptions[channel])
          )
        } else {
          this.removeSubscription(channel, cleanUpQ)
        }
      } else {
        this.removeSubscription(channel, cleanUpQ)
      }
      i++
    }

    if (cleanUpQ.length) {
      await Promise.all(cleanUpQ)
      console.log('cleaned up clients / subscriptions', cleanUpQ.length)
    }
  }

  revalidateSubscriptions() {
    setTimeout(() => {
      this.updateSubscriptionData()
    }, 1000)
    this.revalidateSubscriptionsTimeout = setTimeout(() => {
      this.revalidateSubscriptions()
    }, 10 * 60e3)
  }

  startServerHeartbeat() {
    const setHeartbeat = () => {
      if (this.pub) {
        this.pub.publish(prefixes.serverHeartbeat, String(Date.now()))
      }
      this.serverHeartbeatTimeout = setTimeout(setHeartbeat, 2e3)
    }
    setHeartbeat()
  }

  clear() {
    this.inProgress = {}
    this.clients = {}
    this.subscriptions = {}
    this.cleanUp = false
    clearTimeout(this.revalidateSubscriptionsTimeout)
    clearTimeout(this.serverHeartbeatTimeout)
    this.sub = null
    this.pub = null
    this.sPub = null
    this.sSub = null
  }

  async connect(opts: ConnectOptions): Promise<void> {
    let isResolved = false

    console.log('ConnectOptions SERVER', opts)

    return new Promise(resolve => {
      // make promise assignable as well
      this.client = new SelvaClient(opts, { loglevel: 'info' })

      this.client.on('connect', () => {
        console.log('connect server-client')
        // ------------------------------
        // want to remove this
        this.sub = this.client.redis.redis.sub
        this.pub = this.client.redis.redis.client
        this.sSub = this.client.redis.redis.sSub
        this.sPub = this.client.redis.redis.sClient
        // ------------------------------
        addListeners(this)
        this.startServerHeartbeat()
        this.updateSubscriptionData()
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

  async sendUpdate(subscriptionId: string, deleteOp: boolean = false) {
    return sendUpdate(this, subscriptionId, deleteOp)
  }
}
