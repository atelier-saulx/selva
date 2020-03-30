import Observable from '../observe/observable'
import { GetOptions, GetResult } from '../get/types'
import { LogFn, SelvaOptions, SelvaClient } from '..'
import { createClient, RedisWrapper } from './redisWrapper'
import { Event, RedisCommand } from './types'
import RedisMethods from './redisMethods'
// adds FT commands to redis
import './redisSearch'

type Subscription = {
  channel: string
  getOpts: GetOptions
  count: number
}

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

export default class RedisClient extends RedisMethods {
  public connector: () => Promise<ConnectOptions>
  public isDestroyed: boolean

  private clientId: string
  private log: LogFn | undefined
  private selvaClient: SelvaClient
  private connectOptions: ConnectOptions
  private subscriptions: { [channel: string]: Subscription } = {}

  constructor(
    connect: ConnectOptions | (() => Promise<ConnectOptions>),
    selvaClient: SelvaClient,
    selvaOpts: SelvaOptions
  ) {
    super()
    this.clientId = selvaClient.clientId
    this.selvaClient = selvaClient
    this.log =
      (selvaOpts && selvaOpts.log) ||
      (selvaOpts && selvaOpts.loglevel ? defaultLogging : undefined)
    this.connector =
      typeof connect === 'object' ? () => Promise.resolve(connect) : connect
    this.connect()
  }

  destroy() {
    this.isDestroyed = true
    this.redis.removeClient(this.clientId)
    if (this.redis.sub) {
      this.redis.sub.unsubscribe(`___selva_lua_logs:${this.clientId}`)
    }
    this.redis = null
    this.connected = { client: false, sub: false }
    this.buffer = { client: [], sub: [] }
  }

  createSubscription(channel: string, getOpts: GetOptions) {
    const subscription = (this.subscriptions[channel] = {
      channel,
      getOpts,
      count: 0
    })
    if (this.connected.sub) {
      this.redis.subscribe(this.clientId, channel, getOpts)
    }
    return subscription
  }

  subscribe(channel: string, getOpts: GetOptions): Observable<GetResult> {
    const subscription =
      this.subscriptions[channel] || this.createSubscription(channel, getOpts)
    subscription.count++

    // make this a little bit more efficient
    return new Observable(observer => {
      // add this fn to the total
      // make this more efficient
      const listener = (str: string) => {
        const event: Event = JSON.parse(str)
        if (event.type === 'update') {
          observer.next(event.payload)
        } else if (event.type === 'delete') {
          observer.next(null)
        }
      }
      // subscription.emitter.on('publish', listener)

      // gets here twice not so nice
      return () => {
        subscription.count--
        // subscription.emitter.removeListener('publish', listener)
        if (subscription.count === 0) {
          this.redis.unsubscribe(this.clientId, channel)
          delete this.subscriptions[channel]
        }
      }
    })
  }

  private async registerSubscriptions() {
    for (const channel in this.subscriptions) {
      this.redis.subscribe(
        this.clientId,
        channel,
        this.subscriptions[channel].getOpts
      )
    }
  }

  public redis: RedisWrapper

  public buffer: { client: RedisCommand[]; sub: RedisCommand[] } = {
    client: [],
    sub: []
  }

  public connected: { client: boolean; sub: boolean } = {
    client: false,
    sub: false
  }

  loadAndEvalScript(
    scriptName: string,
    script: string,
    numKeys: number,
    keys: string[],
    args: string[],
    type: string,
    opts?: { batchingEnabled?: boolean }
  ): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue(
        'loadAndEvalScript',
        [
          opts && opts.batchingEnabled ? 1 : 0,
          script,
          scriptName,
          numKeys,
          ...keys,
          ...args
        ],
        resolve,
        reject,
        type
      )
    })
  }

  async queue(
    command: string,
    args: (string | number)[],
    resolve: (x: any) => void = () => {},
    reject: (x: Error) => void = () => {},
    type?: string
  ) {
    // remove type
    if (type === undefined) {
      if (command === 'subscribe') {
        type = 'sub'
      } else {
        type = 'client'
      }
    }
    if (this.connected[type]) {
      this.redis.queue(command, args, resolve, reject, type)
    } else {
      this.buffer[type].push({
        command,
        args,
        resolve,
        reject
      })
    }
  }

  public sendQueueToRedis(type) {
    this.buffer[type].forEach(({ command, args, resolve, reject }) => {
      this.redis.queue(command, args, resolve, reject, type)
    })
    this.buffer[type] = []
  }

  private async connect() {
    if (this.isDestroyed) {
      return
    }
    const opts = await this.connector()
    this.connectOptions = opts
    this.redis = createClient(opts)
    this.registerSubscriptions()

    this.redis.addClient(this.clientId, {
      message: (channel, message) => {
        console.log('publish something for this client', channel, message)
        // recieves publish for all channels this client is interessted in
      },
      log: this.log,
      connect: type => {
        this.connected[type] = true
        this.sendQueueToRedis(type)
        if (this.redis.allConnected) {
          this.selvaClient.emit('connect')
        }
      },
      disconnect: type => {
        this.connected[type] = false
        if (this.redis.allDisconnected) {
          this.selvaClient.emit('disconnect')
        }
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
      },
      client: this
    })
  }
}
