import { createClient, RedisClient as Redis } from 'redis'
import RedisClient from '../redis'
// also needs to handle retry strategy so a little bit different

// const redisClients: Record<string, [number, RedisClient]> = {}

// this.retryTimer = 100
// let tries = 0

// const opts = await this.connector()

// // new redis

// if (this.client) {
// fix it
// this.client.quit()
// this.client = null
// } else {
// this.client = null
// }

// destroy subs manager

// opts.retryStrategy = () => {
//   // console.log('RECON', tries)
//   tries++
//   // needs to re do client
//   // prob want a keep alive thing in here

//   this.resetScripts()
//   this.connected = false
//   this.subscriptionManager.markSubscriptionsClosed()
//   this.connector().then(async newOpts => {
//     if (
//       newOpts.host !== opts.host ||
//       newOpts.port !== opts.port ||
//       tries > 15
//     ) {
//       // console.log('HARD RECONN')
//       // remake client for every connected client

//       // client quit and replace client has to be updated to every client

//       this.client.quit()
//       this.connected = false
//       // does this get recreated?
//       this.subscriptionManager.disconnect()
//       await this.connect()
//     }
//   })
//   if (this.retryTimer < 1e3) {
//     this.retryTimer += 100
//   }
//   return this.retryTimer
// }

// this.client.on('error', err => {
//   // console.log('ERR', err)
//   if (err.code === 'ECONNREFUSED') {
//     console.info(`Connecting to ${err.address}:${err.port}`)
//   } else {
//     // console.log('ERR', err)
//   }
// })

// this.client.on('connect', _ => {
//   tries = 0
//   // console.log('connect it')
// })

// this.client.on('ready', () => {
//   // console.log('ready')
//   tries = 0
//   this.retryTimer = 100
//   this.connected = true
//   this.flushBuffered()
// })

// // this has to be managed better

// class RedisConnector {
//   public redis: Redis
//   constructor(client: RedisClient)) {
//     //   this.redis = new
//   }
//   public removeClient(id: string) {
//     // need a client id
//   }
// }

/*

    this.sub.on('error', err => {
      if (err.code === 'ECONNREFUSED') {
        console.info(`Connecting to ${err.address}:${err.port}`)
      } else {
        // console.log('ERR', err)
      }
    })

    this.pub.on('error', err => {
      if (err.code === 'ECONNREFUSED') {
        console.info(`Connecting to ${err.address}:${err.port}`)
      } else {
        // console.log('ERR', err)
      }
    })

    // its allready ready
    // what about reconn???

    // needs to happend from the register

    

    this.sub.on('ready', () => {
      this.connected = true
      this.ensureSubscriptions()
      this.attachLogging()
    })

    this.pub.on('ready', () => {
      this.connected = true
      this.startHeartbeats()
    })

*/
