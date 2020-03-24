import Observable from '../../observe/observable'
import { createClient, RedisClient as Redis } from 'redis'

import { LogFn } from '../../'

export default class SelvaPubSub {
  private subscriptions: { [channel: string]: RedisSubsription } = {}
  private pub: Redis
  private sub: Redis
  private log: LogFn

  connect(opts: ConnectOptions) {
    this.opts = opts
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
      this.attachLogging()
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

  attachLogging() {
    this.sub.subscribe(`___selva_lua_logs:${this.clientId}`)
  }

  private async ensureSubscriptions() {
    for (const channel in this.subscriptions) {
      if (!this.subscriptions[channel].active) {
        this.sub.subscribe(channel)
        this.subscriptions[channel].active = true

        await this.setSubcriptionData(channel)
      }
    }

    // ensure old listener is gone
    this.sub.removeAllListeners('message')

    this.sub.on('message', (channel, message) => {
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
  }

  // public async unsubscribe (channel: string) {

  // }

  private async setSubcriptionData(channel: string) {
    // this wil change
    try {
      await new Promise((resolve, reject) => {
        const tx = this.pub.multi()
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
            this.attemptReconnect()
            return reject(err)
          }

          this.pub.publish(
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

  async markSubscriptionsClosed() {
    for (const channel in this.subscriptions) {
      this.subscriptions[channel].active = false
    }
  }
}
