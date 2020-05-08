import { RedisClient } from 'redis'
import { RedisCommand } from '../types'
import RedisSelvaClient from '../'
import './redisSearch'
import { ServerType } from '../../types'
import { EventEmitter } from 'events'
import createRedisClient from './createRedisClient'

type ClientOpts = {
  name: string
  type: ServerType
  host: string
  port: number
  id: string
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

    this.subscriber = createRedisClient(this, host, port)
    this.publisher = createRedisClient(this, host, port)

    this.scripts = { batchingEnabled: {}, sha: {} }

    this.serverIsBusy = false

    this.queueInProgress = false
    this.queue = []

    this.connected = false
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

export function removeRedisClient(
  client: Client,
  selvaRedisClient: RedisSelvaClient
) {}

export function addRedisClient(
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
