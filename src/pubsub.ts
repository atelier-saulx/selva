import Observable from './observe/observable'
import { createClient, RedisClient as Redis } from 'redis'
import { EventEmitter } from 'events'
import { ConnectOptions } from './redis'

type RedisSubsription = {
  channel: string
  active: boolean
  emitter: EventEmitter
}

export default class SelvaPubSub {
  private subscriptions: { [channel: string]: RedisSubsription } = {}
  private pub: Redis
  private sub: Redis
  private heartbeatTimer: NodeJS.Timeout
  private connected: boolean = false

  connect(opts: ConnectOptions) {
    this.pub = createClient(opts)
    this.sub = createClient(opts)

    this.sub.on('error', err => {
      // console.log('ERRRRR')
      if (err.code === 'ECONNREFUSED') {
        console.info(`Connecting to ${err.address}:${err.port}`)
      } else {
        // console.log('ERR', err)
      }
    })

    this.pub.on('error', err => {
      // console.log('ERRRRR')
      if (err.code === 'ECONNREFUSED') {
        console.info(`Connecting to ${err.address}:${err.port}`)
      } else {
        // console.log('ERR', err)
      }
    })

    this.sub.on('ready', () => {
      this.connected = true
      this.ensureSubscriptions()
    })

    this.pub.on('ready', () => {
      this.connected = true
      this.startHeartbeats()
    })
  }

  disconnect() {
    this.stopHeartbeats()
    this.markSubscriptionsClosed()

    this.connected = false
    if (this.pub) {
      this.pub.quit()
    }

    if (this.sub) {
      this.sub.quit()
    }

    this.pub = this.sub = undefined
  }

  subscribe(channel: string) {
    const current = this.subscriptions[channel]
    if (current && current.active) {
      return
    }

    const emitter = new EventEmitter()

    this.subscriptions[channel] = {
      channel,
      active: this.connected,
      emitter
    }

    if (this.connected) {
      this.sub.subscribe(channel)
    }

    return new Observable(observer => {
      emitter.on('publish', str => {
        observer.next(str)
      })

      return () => {
        this.sub.unsubscribe('channel')
        delete this.subscriptions[channel]
      }
    })
  }

  private startHeartbeats() {
    const timeout = () => {
      console.log('heartbeats')
      this.heartbeatTimer = setTimeout(() => {
        if (this.connected && this.pub) {
          for (const channel in this.subscriptions) {
            this.pub.publish('___selva_subscription:client_heartbeats', channel)
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

  private async ensureSubscriptions() {
    for (const channel in this.subscriptions) {
      if (!this.subscriptions[channel].active) {
        this.sub.subscribe(channel)
        this.subscriptions[channel].active = true
      }
    }

    // ensure old listener is gone
    this.sub.removeAllListeners('message')

    this.sub.on('message', (channel, message) => {
      const sub = this.subscriptions[channel]
      if (sub) {
        sub.emitter.emit('publish', message)
      }
    })
  }

  async markSubscriptionsClosed() {
    for (const channel in this.subscriptions) {
      this.subscriptions[channel].active = false
    }
  }
}
