import { RedisClient } from 'redis'
import { SelvaClient, GetOptions, Schema } from '@saulx/selva'

import addFields from './addFields'
import addListeners from './addListeners'
import sendUpdate from './sendUpdate'

export type QuerySubscription = {
  idFields?: Record<string, true>
  queryId: string
  ids?: Record<string, true>
  member: { $field: string; $value: string[] }[] // array is an OR
  type?: string[]
  fields: {
    [key: string]: true
  }
  time?: { nextRefresh: number }
}

export default class SubscriptionManager {
  public queries: Record<string, QuerySubscription[]> = {}
  public nowBasedQueries: {
    nextRefresh: number
    queries: { subId: string; nextRefresh: number }[]
  }

  public refsById: Record<string, Record<string, string>> = {}

  public client: SelvaClient
  public sub: RedisClient
  public pub: RedisClient // pub and client

  public inProgress: Record<string, true> = {}
  public incomingCount: number = 0
  public isDestroyed: boolean = false
  public cleanUp: boolean = false

  public refreshNowQueriesTimeout: NodeJS.Timeout

  public lastResultHash: Record<string, string> = {}

  // to check if the server is still ok
  public serverHeartbeatTimeout: NodeJS.Timeout

  // revalidates subs ones in a while
  public revalidateSubscriptionsTimeout: NodeJS.Timeout

  // just set this from the client
  // subscribed on newSubscription
  public clients: Record<
    string,
    { lastTs: number; subscriptions: Set<string> }
  > = {}
  public subscriptions: Record<
    string,
    { clients: Set<string>; get: GetOptions; version?: string }
  > = {}

  // public subscriptions: Record<string, GetOptions> = {}
  // have to update a put

  public subscriptionsByField: Record<string, Set<string>> = {}

  // update individual, update non indivdual

  async addClientSubscription(client: string, channel: string) {
    const subscriptionsName = '___selva_subscriptions'

    // just to speed things up and potentialy send something
    if (!this.subscriptions[channel]) {
      console.log('channel is not there lets make make', channel)
      const [getOptions, clients] = await Promise.all([
        this.client.redis.hget(subscriptionsName, channel),
        this.client.redis.smembers(channel)
      ])
      if (getOptions && clients.length) {
        this.addSubscription(channel, new Set(clients), getOptions)
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
    const clients = await this.client.redis.smembers(channel)
    let len = clients.length
    const cIndex = clients.indexOf(client)
    if (cIndex !== -1) {
      len--
      cleanUpQ.push(this.client.redis.srem(channel, client))
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
      console.log(
        'cleaned up subscriptions from removeClientSubscription',
        cleanUpQ.length
      )
    }
  }

  addSubscription(
    channel: string,
    clients: Set<string>,
    getOptions: GetOptions
  ) {
    this.subscriptions[channel] = {
      clients,
      get: getOptions
    }

    // have to make version as well - and store it somewhere probably
    // this piece can later be replaced with a seperate SUBSCRIPTION SERVER
    // all subscriptions etc by clients will be done on this server
    // means we make more clients on the client side (1 extra)
    // subscriptions (the actual publishes etc) can then be made on this on as well
  }

  removeSubscription(channel: string, cleanUpQ: any[] = []) {
    const subscriptionsName = '___selva_subscriptions'
    cleanUpQ.push(this.client.redis.hdel(subscriptionsName, channel))
    cleanUpQ.push(this.client.redis.del(channel))

    // want to clean up the db
  }

  async updateSubscriptionData() {
    // in progress as well
    // has to become lua
    const subscriptionsName = '___selva_subscriptions'
    const clientsName = `___selva_clients`

    // can do multi if you want
    const [subscriptions, clients] = await Promise.all([
      this.client.redis.hgetall(subscriptionsName),
      this.client.redis.hgetall(clientsName)
    ])

    const q = []
    for (const channel in subscriptions) {
      q.push(this.client.redis.smembers(channel))
    }

    const subsClients = await Promise.all(q)

    this.clients = {}
    this.subscriptions = {}

    const cleanUpQ = []

    const now = Date.now()
    for (const client in clients) {
      const ts = Number(clients[client])
      if (now - ts < 5e3) {
        this.clients[client] = {
          lastTs: ts,
          subscriptions: new Set()
        }
      } else {
        console.log('Client is timedout', client)
        cleanUpQ.push(this.client.redis.hdel(clientsName, client))
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
            cleanUpQ.push(this.client.redis.srem(channel, client))
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
    this.updateSubscriptionData()
    this.revalidateSubscriptionsTimeout = setTimeout(() => {
      this.revalidateSubscriptions()
    }, 3e3)
  }

  startServerHeartbeat() {
    const heartbeatChannel = '___selva_subscription:server_heartbeat'
    const setHeartbeat = () => {
      this.pub.publish(heartbeatChannel, String(Date.now()))
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
  }

  connect(port: number): Promise<void> {
    let isResolved = false
    return new Promise((resolve, reject) => {
      this.client = new SelvaClient({ port }, { loglevel: 'info' })

      this.client.on('connect', () => {
        console.log('connect server-client')
        // ------------------------------
        // want to remove this
        this.sub = this.client.redis.redis.sub
        this.pub = this.client.redis.redis.client
        // ------------------------------
        addListeners(this)
        this.revalidateSubscriptions()
        this.startServerHeartbeat()
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
    this.isDestroyed = true
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

  async sendUpdate(
    subscriptionId: string,
    getOptions?: GetOptions,
    deleteOp: boolean = false
  ) {
    return sendUpdate(this, subscriptionId, getOptions, deleteOp)
  }
}
