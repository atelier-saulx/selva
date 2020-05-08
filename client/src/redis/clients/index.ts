import { RedisClient } from 'redis'
import { RedisCommand, Client } from '../types'
import Redis from '../'
import './redisSearch'
import { ServerType } from '../../types'

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

const createRedisClient = (port, host): RedisClient => {
  // add reconn options on the redis clients!
  return new RedisClient({ port, host })
}

const createClient = (
  name: string,
  type: ServerType,
  id: string,
  port: number,
  host: string
): Client => {
  const client: Client = {
    name,
    type,
    id,
    clients: new Set(),
    subscriber: createRedisClient(port, host),
    publisher: createRedisClient(port, host),
    scripts: {
      batchingEnabled: {},
      sha: {}
    },
    busy: false,
    queueInProgress: false,
    queue: [],
    connected: false
  }

  console.log('create client')

  return client
}

// removeRedisClient (from client)
// addRedisClient (from client)

export function addRedisClient() {}

export function getClient(
  selvaRedisClient: Redis,
  name: string,
  type: ServerType,
  port: number,
  url: string = '0.0.0.0'
) {
  const id = url + port
  let client = clients.get(id)
  if (!client) {
    createClient(name, type, id, port, url)
  }
}
