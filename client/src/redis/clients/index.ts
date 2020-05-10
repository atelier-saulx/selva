import { RedisClient } from 'redis'
import { RedisCommand } from '../types'
import RedisSelvaClient from '../'
import './redisSearch'
import { ServerType } from '../../types'
import { EventEmitter } from 'events'
import createRedisClient from './createRedisClient'
import execBatch from './execBatch'

type ClientOpts = {
  name: string
  type: ServerType
  host: string
  port: number
  id: string
}

const drainQueue = (client: Client, q = client.queue) => {
  if (!client.queueInProgress) {
    client.queueInProgress = true
    process.nextTick(() => {
      if (client.connected) {
        let nextQ: RedisCommand[]
        const parsedQ = []
        for (let i = 0; i < q.length; i++) {
          console.log(i)
          const redisCommand = q[i]
          const { command, resolve, args } = redisCommand
          if (command === 'subscribe') {
            client.subscriber.subscribe(...(<string[]>args))
            resolve(true)
          } else if (command === 'psubscribe') {
            client.subscriber.psubscribe(...(<string[]>args))
            resolve(true)
          } else {
            // is it load and eval script time?
            // TODO: TONY magic 🍕
            // esle
            parsedQ.push(redisCommand)
            if (parsedQ.length >= 5e3) {
              nextQ = q.slice(i)
              break
            }
          }
        }
        client.queue = []
        execBatch(client, parsedQ).finally(() => {
          if (nextQ) {
            drainQueue(client, nextQ)
          } else if (client.queue.length) {
            drainQueue(client, client.queue)
          } else {
            client.queueInProgress = false
          }
        })
      } else {
        client.queueInProgress = false
        console.log('Not connected wait a little bit')
      }
    })
  }
}

export class Client extends EventEmitter {
  public subscriber: RedisClient
  public publisher: RedisClient
  public queue: RedisCommand[]
  public queueInProgress: boolean
  public name: string // for logging
  public type: ServerType // for logs
  public id: string // url:port
  public connected: boolean
  public serverIsBusy: boolean // can be written from the registry
  public scripts: {
    batchingEnabled: { [scriptSha: string]: boolean }
    sha: { [scriptName: string]: string }
  }
  public clients: Set<RedisSelvaClient>
  public heartbeatTimout?: NodeJS.Timeout

  constructor({ name, type, host, port, id }: ClientOpts) {
    super()
    this.setMaxListeners(10000)

    this.name = name
    this.type = type
    this.id = id

    this.clients = new Set()

    this.scripts = { batchingEnabled: {}, sha: {} }

    this.serverIsBusy = false

    this.queueInProgress = false
    this.queue = []

    this.connected = false

    this.on('connect', () => {
      console.log('CONNECT!')

      this.connected = true
      drainQueue(this)
    })

    this.on('disconnect', () => {
      this.connected = false
      this.queueInProgress = false
    })

    this.subscriber = createRedisClient(this, host, port, 'subscriber')
    this.publisher = createRedisClient(this, host, port, 'publisher')

    this.publisher.on('message', v => {
      console.log(v)
    })
  }
}

const clients: Map<string, Client> = new Map()

// addSelvaClient  (createClient(type, id, selvaClient))
// removeSelvaClient

// addCommandToQueue
// --- in coming add command queue ---
// handle these special
// subscribe
// unsubscribe
// psubscribe
// punsubcribe

// selvaSubscribe
// selvaUnsubscribe
// loadAndEvalScript

// export type Client = {
//     subscriber: RedisClient
//     publisher: RedisClient
//     queue: RedisCommand[]

//     name: string // for logging
//     type: ServerType // for logging as well

// url: string
// port: string
//     id: string // url:port
//     connected: boolean
//     bufferInProgress: boolean
//     busy: boolean // can be written from the registry
//     heartbeatTimout: NodeJS.Timeout

//     scripts: {
//       batchingEnabled: { [scriptSha: string]: boolean }
//       sha: { [scriptName: string]: string }
//     }

//     clients: Set<Redis>
//   }

const destroyClient = () => {}

const createClient = (
  name: string,
  type: ServerType,
  id: string,
  port: number,
  host: string
): Client => {
  const client: Client = new Client({
    id,
    name,
    type,
    port,
    host
  })
  console.log('create client')
  return client
}

export function removeRedisSelvaClient(
  client: Client,
  selvaRedisClient: RedisSelvaClient
) {}

export function addRedisSelvaClient(
  client: Client,
  selvaRedisClient: RedisSelvaClient
) {}

export function getClient(
  selvaRedisClient: RedisSelvaClient,
  name: string,
  type: ServerType,
  port: number,
  url: string = '0.0.0.0'
) {
  const id = url + port
  let client = clients.get(id)
  if (!client) {
    client = createClient(name, type, id, port, url)
  }

  return client
}

export function addCommandToQueue(client: Client, redisCommand: RedisCommand) {
  client.queue.push(redisCommand)
  console.log('SNURKY')
  if (!client.queueInProgress) {
    drainQueue(client)
  }
}
