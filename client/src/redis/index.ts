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
    this.connector =
      typeof connect === 'object' ? () => Promise.resolve(connect) : connect
    this.connect()
  }

  destroy() {
    this.isDestroyed = true
    this.redis.removeClient(this.clientId)
    this.redis = null
    this.connected = { client: false, sub: false }
    // send all unsubscribe events
    this.buffer = { client: [], sub: [] }
  }

  private attachLogging() {
    this.queue('subscribe', [`___selva_lua_logs:${this.clientId}`])
  }

  public async unsubscribe(channel: string) {
    // and getOptions as well
    // not used with observables!
    console.log('unsubscribe?', channel)
  }

  createSubscription(channel: string, getOpts: GetOptions) {
    const subscription = (this.subscriptions[channel] = {
      channel,
      emitter: new EventEmitter(),
      getOpts,
      count: 0
    })
    this.queue('subscribe', [channel])
    if (this.connected.client) {
      this.lastHeartbeat[channel] = Date.now()
      this.setSubcriptionData(channel)
    }
    return subscription
  }

  subscribe(channel: string, getOpts: GetOptions): Observable<GetResult> {
    return new Observable(observer => {
      const subscription =
        this.subscriptions[channel] || this.createSubscription(channel, getOpts)
      subscription.count++

      const listener = (str: string) => {
        console.log(str)
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
      return () => {
        subscription.count--
        subscription.emitter.removeListener('publish', listener)
        if (subscription.count === 0) {
          console.log(
            'REMOVE SUBS LISTENER AND SEND IT AS WELL, REMOVE HEATHBEATH ALSO'
          )
          // also remove the hearthbeat
          this.redis.client.unsubscribe(channel)
          delete this.lastHeartbeat[channel]
          delete this.subscriptions[channel]
        }
      }
    })
  }

  private async setSubcriptionData(channel: string) {
    try {
      // dont rly like this - why is this nessecary?
      await new Promise((resolve, reject) => {
        const tx = this.redis.client.multi()
        // combine for each sub
        tx.hset(
          '___selva_subscriptions',
          channel.substr('___selva_subscription:'.length),
          JSON.stringify(this.subscriptions[channel].getOpts)
        )

        // can be done after all subs
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
            JSON.stringify({ channel, refresh: true })
          )
          resolve()
        })
      })
    } catch (e) {
      console.error(e)
      return
    }
  }

  private resendSubscriptions() {
    let q = []
    for (const channel in this.subscriptions) {
      // first check if its not in the queue

      console.log('re-send this guys', 'check if it exists')
      this.redis.sub.subscribe(channel)
      q.push(this.setSubcriptionData(channel))
    }

    // ensure old listener is gone
    this.redis.sub.removeAllListeners('message')

    this.redis.sub.on('message', (channel, message) => {
      console.log('yesh resend new do')
      // does this allways happen now?
      if (channel.startsWith('___selva_lua_logs:') && this.log) {
        this.log(JSON.parse(message))
        return
      }

      const sub = this.subscriptions[channel]

      if (sub) {
        sub.emitter.emit('publish', message)
      }
    })

    Promise.all(q).then(() => {
      console.log('all good i set all the data on subs')
    })
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
            console.log('its been long scince hearthbeath what to do?')
            this.redis.reconnect()
          }
        }

        // ----------------- reconnect only for client ------------
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
    this.redis = createClient(opts)
    this.redis.addClient(this.clientId, {
      connect: type => {
        console.log('CONNECTED', type)
        this.connected[type] = true
        if (type === 'sub') {
          this.attachLogging()
          this.resendSubscriptions()
        } else {
          this.startHeartbeats()
        }
        this.resetScripts(type)
        this.flushBuffered(type)
      },
      disconnect: type => {
        console.log('DIS-CONNECTED', type)
        this.connected[type] = false
        this.connector().then(opts => {
          console.log(
            'DC NEED TO CHECK IF DIFFERENT',
            opts,
            'vs',
            this.connectOptions
          )
        })
      }
    })
  }
}
