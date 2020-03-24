import { createClient as createRedisClient, RedisClient } from 'redis'
import { EventEmitter } from 'events'
import { ConnectOptions } from './'

const redisClients: Record<string, RedisWrapper> = {}

type Listeners = {
  connect: (string) => void
  disconnect: (string) => void
}

export class RedisWrapper extends EventEmitter {
  public client: RedisClient
  public sub: RedisClient
  public id: string
  public noSubscriptions: boolean = false
  public clients: Map<string, Listeners> = new Map()
  public opts: ConnectOptions
  public types: string[]
  public connected: {
    client: boolean
    sub: boolean
  } = {
    client: false,
    sub: false
  }
  private retryTimer: number = 100

  constructor(opts: ConnectOptions, id: string, noSubscriptions?: boolean) {
    super()
    this.opts = opts
    this.id = id

    this.types = noSubscriptions ? ['client'] : ['sub', 'client']

    console.log('create wrapper')
    this.connect()
  }

  public connect() {
    console.log('hello connect it!')
    this.types.forEach(type => {
      let tries = 0
      const typeOpts = Object.assign({}, this.opts, {
        retryStrategy: () => {
          tries++
          if (tries > 15) {
            this.reconnect()
          } else {
            if (tries === 0 && this.connected[type] === true) {
              this.connected[type] = false
              this.emit('disconnect', type)
            }
          }
          this.connected[type] = false
          if (this.retryTimer < 1e3) {
            this.retryTimer += 100
          }
          return this.retryTimer
        }
      })

      const client = (this[type] = createRedisClient(typeOpts))

      client.on('ready', () => {
        tries = 0
        this.connected[type] = true
        this.emit('connect', type)
      })

      client.on('error', err => {
        if (err.code === 'ECONNREFUSED') {
          if (this.connected[type]) {
            console.info(`DC ERROR - ${err.address}:${err.port} ${type}`)
            this.connected[type] = false
            this.emit('disconnect', type)
          }
        }
      })
    })
  }

  public disconnect() {
    this.types.forEach(type => {
      this[type].end(true)
      this.connected[type] = false
      this.emit('disconnect', type)
    })
  }

  public reconnect() {
    this.disconnect()
    setTimeout(() => {
      this.connect()
    }, 1e3)
  }

  public removeClient(client: string) {
    const listeners = this.clients.get(client)
    if (listeners) {
      this.clients.delete(client)
      for (const key in listeners) {
        this.removeListener(key, listeners[key])
      }
    }
    if (this.clients.size === 0) {
      console.log('REMOVE REDIS-WRAPPER', this.id)
      this.client.end(true)
      this.sub.end(true)
      this.client = null
      this.sub = null
      delete redisClients[this.id]
    }
  }

  public addClient(client: string, listeners: Listeners) {
    console.log('add client', client)
    this.clients.set(client, listeners)
    this.types.forEach(type => {
      if (this.connected[type]) {
        listeners.connect(type)
      }
    })
    this.on('connect', listeners.connect)
    this.on('disconnect', listeners.disconnect)
  }
}

export const createClient = (opts, noSubscriptions?: boolean) => {
  const id = `${opts.host || '0.0.0.0'}:${opts.port}:${noSubscriptions ? 1 : 0}`
  if (redisClients[id]) {
    return redisClients[id]
  } else {
    const wrapper = new RedisWrapper(opts, id, noSubscriptions)
    redisClients[id] = wrapper
    console.log('hello re-use that client!', id)
    return wrapper
  }
}
