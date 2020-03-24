import { createClient as createRedisClient, RedisClient } from 'redis'
import { EventEmitter } from 'events'
import { ConnectOptions } from './'

const redisClients: Record<string, RedisWrapper> = {}

const types = ['sub', 'client']

type Listeners = {
  connect: (string) => void
  disconnect: (string) => void
}

export class RedisWrapper extends EventEmitter {
  public client: RedisClient
  public sub: RedisClient
  public id: string
  public clients: Map<string, Listeners> = new Map()
  public opts: ConnectOptions
  public connected: {
    client: boolean
    sub: boolean
  } = {
    client: false,
    sub: false
  }
  private retryTimer: number = 100

  constructor(opts: ConnectOptions) {
    super()
    this.opts = opts
    this.id = `${opts.host || '0.0.0.0'}:${opts.port}`
    this.connect()
  }

  public connect() {
    console.log('hello connect it!')
    types.forEach(type => {
      let tries = 0
      const typeOpts = Object.assign({}, this.opts, {
        retryStrategy: () => {
          tries++
          if (tries > 15) {
            this.reconnect()
          } else {
            // only fire it on tries === 0 and it was connected
            if (tries === 0 && this.connected[type] === true) {
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
        console.log('hello ready')
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
    // not really nice to use :D
    this.client.end(true)
    this.sub.end(true)
    types.forEach(type => {
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
    this.clients.set(client, listeners)
    types.forEach(type => {
      if (this.connected[type]) {
        listeners.connect(type)
      }
    })
    this.on('connect', listeners.connect)
    this.on('disconnect', listeners.disconnect)
  }
}

export const createClient = opts => {
  const id = `${opts.host || '0.0.0.0'}:${opts.port}`
  if (redisClients[id]) {
    return redisClients[id]
  } else {
    const wrapper = new RedisWrapper(opts)
    redisClients[id] = wrapper
    console.log('hello re-use that client!', id)
    return wrapper
  }
}
