import Observable from '../observe/observable'
import Queue from './queue'
import { GetOptions, GetResult } from '../get/types'
import { EventEmitter } from 'events'
import { LogFn, SelvaOptions } from '../'
import { createClient } from './redisWrapper'
import { Subscription, Event } from './types'
// adds FT commands
import './redisSearch'

export type ConnectOptions = {
  port: number
  host?: string
}

const defaultLogging: LogFn = log => {
  if (log.level === 'warning') {
    console.warn('LUA: ' + log.msg)
  } else {
    console[log.level]('LUA: ' + log.msg)
  }
}

export default class RedisClient extends Queue {
  public connector: () => Promise<ConnectOptions>
  public isDestroyed: boolean
  public noSubscriptions: boolean = false
  private clientId: string
  private log: LogFn
  private connectOptions: ConnectOptions
  private subscriptions: { [channel: string]: Subscription } = {}
  private lastHeartbeat: { [channel: string]: number } = {}
  private heartbeatTimer: NodeJS.Timeout

  constructor(
    connect: ConnectOptions | (() => Promise<ConnectOptions>),
    clientId: string,
    selvaOpts: SelvaOptions
  ) {
    super()
    this.clientId = clientId
    this.log = (selvaOpts && selvaOpts.log) || defaultLogging
    this.noSubscriptions = selvaOpts && selvaOpts.noSubscriptions
    this.connector =
      typeof connect === 'object' ? () => Promise.resolve(connect) : connect
    this.connect()
  }

  destroy() {
    this.isDestroyed = true
    this.redis.removeClient(this.clientId)
    this.redis = null
    this.connected = { client: false, sub: false }
    // send all unsubscribe events as well (for faster unsubscribe)
    this.buffer = { client: [], sub: [] }
  }

  private attachLogging() {
    this.redis.sub.subscribe(`___selva_lua_logs:${this.clientId}`)
  }

  public async unsubscribe(channel: string) {
    console.warn('Unsubscribe subs not implemented!', channel)
  }

  createSubscription(channel: string, getOpts: GetOptions) {
    const subscription = (this.subscriptions[channel] = {
      channel,
      emitter: new EventEmitter(),
      getOpts,
      count: 0
    })
    if (this.connected.sub) {
      this.redis.sub.subscribe(channel)
      this.lastHeartbeat[channel] = Date.now()
    }
    return subscription
  }

  subscribe(channel: string, getOpts: GetOptions): Observable<GetResult> {
    const subscription =
      this.subscriptions[channel] || this.createSubscription(channel, getOpts)
    subscription.count++

    return new Observable(observer => {
      const listener = (str: string) => {
        const event: Event = JSON.parse(str)
        if (event.type === 'update') {
          observer.next(event.payload)
        } else if (event.type === 'delete') {
          observer.next(null)
        } else if (event.type === 'heartbeat') {
          this.lastHeartbeat[channel] = Date.now()
        }
      }

      subscription.emitter.on('publish', listener)

      if (this.connected.sub && this.connected.client) {
        // not so nice to reset data ont he server for every multiplexed sub
        this.setSubcriptionData(channel)
      }

      return () => {
        subscription.count--
        subscription.emitter.removeListener('publish', listener)
        if (subscription.count === 0) {
          this.redis.sub.unsubscribe(channel)
          delete this.lastHeartbeat[channel]
          delete this.subscriptions[channel]
        }
      }
    })
  }

  private async setSubcriptionData(channel: string) {
    try {
      // combine all subs
      await new Promise((resolve, reject) => {
        if (this.connected.client) {
          const tx = this.redis.client.multi()
          tx.hset(
            '___selva_subscriptions',
            channel.substr('___selva_subscription:'.length),
            JSON.stringify(this.subscriptions[channel].getOpts)
          )
          tx.hset(
            '___selva_subscriptions',
            '___lastEdited',
            new Date().toISOString()
          )
          tx.exec((err, _replies) => {
            if (err) {
              this.redis.reconnect()
              return reject(err)
            }
            this.redis.client.publish(
              '___selva_subscription:client_heartbeats',
              JSON.stringify({ channel, refresh: true }),
              err => {
                // console.log('publish callback', err)
              }
            )
            resolve()
          })
        } else {
          reject()
        }
      })
    } catch (e) {
      console.error(e)
      return
    }
  }

  private async resendSubscriptions() {
    let q = []
    for (const channel in this.subscriptions) {
      this.redis.sub.subscribe(channel)
      this.lastHeartbeat[channel] = Date.now()
      q.push(this.setSubcriptionData(channel))
    }

    // ensure old listener is gone
    this.redis.sub.removeAllListeners('message')
    this.redis.sub.on('message', (channel, message) => {
      if (channel.startsWith('___selva_lua_logs:') && this.log) {
        this.log(JSON.parse(message))
        return
      }
      const sub = this.subscriptions[channel]
      if (sub) {
        sub.emitter.emit('publish', message)
      }
    })

    await Promise.all(q)
  }

  private startHeartbeats() {
    if (this.heartbeatTimer) {
      return
    }
    const timeout = () => {
      this.heartbeatTimer = setTimeout(() => {
        for (const channel in this.lastHeartbeat) {
          if (
            this.lastHeartbeat[channel] &&
            Date.now() - this.lastHeartbeat[channel] > 1000 * 60
          ) {
            this.redis.reconnect()
          }
        }
        if (this.connected.client) {
          for (const channel in this.subscriptions) {
            this.redis.client.publish(
              '___selva_subscription:client_heartbeats',
              JSON.stringify({ channel })
            )
          }
        } else {
          this.stopHeartbeats()
          return
        }
        timeout()
      }, 1000 * 30)
    }
    timeout()
  }

  private stopHeartbeats() {
    if (this.heartbeatTimer) {
      clearTimeout(this.heartbeatTimer)
      this.heartbeatTimer = undefined
    }
  }

  private async connect() {
    if (this.isDestroyed) {
      return
    }
    const opts = await this.connector()
    this.connectOptions = opts
    this.redis = createClient(opts, this.noSubscriptions)
    this.redis.addClient(this.clientId, {
      connect: type => {
        this.connected[type] = true
        if (this.connected.sub && this.connected.client) {
          this.attachLogging()
          this.startHeartbeats()
          this.resendSubscriptions()
        }
        this.resetScripts(type)
        this.flushBuffered(type)
      },
      disconnect: type => {
        this.connected[type] = false
        if (type === 'client') {
          this.connector().then(opts => {
            if (
              opts.host !== this.connectOptions.host ||
              opts.port !== this.connectOptions.port
            ) {
              this.redis.removeClient(this.clientId)
              this.connect()
            }
          })
        }
      }
    })
  }
}
