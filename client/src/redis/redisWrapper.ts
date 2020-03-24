import { createClient as createRedisClient, RedisClient } from 'redis'
import { EventEmitter } from 'events'

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
  public connected: {
    client: boolean
    sub: boolean
  } = {
    client: false,
    sub: false
  }
  private retryTimer: number = 100
  private heartbeatTimer: NodeJS.Timeout
  private lastHeartbeat: { [channel: string]: number } = {}

  constructor(opts) {
    super()
    this.id = opts.host + opts.port
    types.forEach(type => {
      const typeOpts = Object.assign({}, opts, {
        retryStrategy: () => {
          // this.emit('disconnect', )
          // console.log('RECON', tries)
          //   tries++
          // needs to re do client
          // prob want a keep alive thing in here
          //   this.resetScripts()
          //   this.connected = false
          //   this.subscriptionManager.markSubscriptionsClosed()
          //   this.connector().then(async newOpts => {
          // if (
          //   newOpts.host !== opts.host ||
          //   newOpts.port !== opts.port ||
          //   tries > 15
          // ) {
          //   // console.log('HARD RECONN')
          //   this.client.quit()
          //   this.connected = false
          //   this.subscriptionManager.disconnect()
          //   await this.connect()
          // }
          //   })

          // after some retires fire dc again

          if (this.retryTimer < 1e3) {
            this.retryTimer += 100
          }
          return this.retryTimer
        }
      })

      const client = (this[type] = createRedisClient(typeOpts))

      client.on('ready', () => {
        this.connected[type] = true
        this.emit('connect', type)
      })

      client.on('error', err => {
        if (err.code === 'ECONNREFUSED') {
          // else its just first connection its fine
          if (this.connected[type]) {
            console.info(
              `disconnect event - ${err.address}:${err.port} ${type}`
            )
            this.connected[type] = false
            this.emit('disconnect', type)
          }
        } else {
          // console.log('ERR', err)
        }
      })
    })
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
      this.client.quit()
      this.sub.quit()
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

  private stopHeartbeats() {
    if (this.heartbeatTimer) {
      clearTimeout(this.heartbeatTimer)
      this.heartbeatTimer = undefined
    }
  }

  private startHeartbeats() {
    if (this.heartbeatTimer) {
      return
    }

    const timeout = () => {
      this.heartbeatTimer = setTimeout(() => {
        // hearthbeat sits on every subscription
        // so need to subscribe on 'hearthbeat' allways else it dc's
        for (const channel in this.lastHeartbeat) {
          if (
            this.lastHeartbeat[channel] &&
            Date.now() - this.lastHeartbeat[channel] > 1000 * 60
          ) {
            // it's been too long since latest server heartbeat, disconnecting and connecting again in a second
            // this.attemptReconnect()
          }
        }

        // console.log('HEARTHBEAT GO')

        // just emit

        // if (this.connected && this.pub) {
        //   for (const channel in this.subscriptions) {
        //     this.pub.publish(
        //       '___selva_subscription:client_heartbeats',
        //       JSON.stringify({ channel })
        //     )
        //   }
        // } else {
        //   this.stopHeartbeats()
        //   return
        // }

        timeout()
        // every 30 sec
      }, 1000 * 30)
    }

    timeout()
  }
}

export const createClient = opts => {
  const id = opts.host + opts.port
  if (redisClients[id]) {
    return redisClients[id]
  } else {
    const wrapper = new RedisWrapper(opts)
    redisClients[id] = wrapper
    return wrapper
  }
}
