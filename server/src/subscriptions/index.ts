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
    { lastTs: number; subscriptions: string[] }
  > = {}
  public subscriptions: Record<
    string,
    { clients: Set<string>; get?: GetOptions; version?: string }
  > = {}

  // public subscriptions: Record<string, GetOptions> = {}
  // have to update a put

  public subscriptionsByField: Record<string, Set<string>> = {}

  // update individual, update non indivdual

  addSubscription(client: string, channel: string) {}

  removeSubscription(client: string, channel: string) {}

  updateSubscriptionData() {
    // console.log('updateSubscriptionData', client, channel)
    // also check if the server has timeoud
    // checks clients
    // checks clients hearthbeats
    // checks subscriptions
    // hexists(subscriptions, channel)
    // hset(subscriptions, channel: getOptions)
    // client is redisWrapper client
    // sadd(clients, client)
    // sadd(client, channel)
  }

  revalidateSubscriptions() {
    // console.log('\nrevalidate those subs')
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
        // have to check everuywhere if sub exists
        this.sub = this.client.redis.redis.sub
        this.pub = this.client.redis.redis.client
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
