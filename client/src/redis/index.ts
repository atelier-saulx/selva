import Observable from '../observe/observable'
import Queue from './queue'
import { GetOptions } from '../get/types'
import { EventEmitter } from 'events'
import { LogFn, SelvaOptions } from '../'
import { createClient } from './redisWrapper'
// adds FT commands
import './redisSearch'

// ok use this string (host + port)

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
  //   public client: Redis // different client its a wrapper with (client|pub) / sub
  public isDestroyed: boolean
  private clientId: string
  private log: LogFn
  private connectOptions: ConnectOptions

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
    this.buffer = { client: [], sub: [] }
  }

  public async unsubscribe(channel: string) {
    // and getOptions as well
  }

  subscribe<T>(channel: string, getOpts: GetOptions): Observable<T> {
    const emitter = new EventEmitter()
    console.log('subscribe on channel', channel)

    // this.subscriptions[channel] = {
    //   channel,
    //   active: this.connected,
    //   emitter,
    //   getOpts
    // }

    // if (this.connected) {
    //   this.sub.subscribe(channel)
    //   this.lastHeartbeat[channel] = Date.now()
    //   this.setSubcriptionData(channel)
    // }

    return new Observable(observer => {
      emitter.on('publish', str => {
        const event: Event = JSON.parse(str)

        if (event.type === 'update') {
          //   observer.next(event.payload)
        } else if (event.type === 'delete') {
          observer.next(null)
        } else if (event.type === 'heartbeat') {
          //   this.lastHeartbeat[channel] = Date.now()
        }
      })

      return () => {
        // this is the cleanup
        console.log('REMOVE SUBS LISTENER')
        // this.sub.unsubscribe(channel)
        // delete this.subscriptions[channel]
        // delete this.lastHeartbeat[channel]
      }
    })
  }

  private async connect() {
    if (this.isDestroyed) {
      return
    }

    const opts = await this.connector()

    this.connectOptions = opts

    console.log('here', this.connectOptions, this.clientId)

    this.redis = createClient(opts)

    this.redis.addClient(this.clientId, {
      connect: type => {
        console.log('CONNECTED', type)
        this.connected[type] = true
        // have to do much more ofc
      },
      disconnect: type => {
        console.log('DIS-CONNECTED', type)
        this.connected[type] = false
        // have to do much more ofc
      }
    })
  }

  private attachLogging() {
    // subs are different scince you need to reset when it reconns!
    // add this in queue
    // keep track of them
    // unsusbscribe has to be handled the same
    // optional get
    this.queue(
      'subscribe',
      [`___selva_lua_logs:${this.clientId}`],
      () => {
        console.log('subcribed to logs')
      },
      err => {
        console.error('cannot subscribe logs')
      }
    )
    // this.sub.subscribe(`___selva_lua_logs:${this.clientId}`)
  }
}
