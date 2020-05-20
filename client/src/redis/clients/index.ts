import { RedisClient } from 'redis'
import { RedisCommand } from '../types'
import RedisSelvaClient from '../'
import './redisSearch'
import { ServerType, ServerDescriptor } from '../../types'
import { EventEmitter } from 'events'
import createRedisClient from './createRedisClient'
import { loadScripts } from './scripts'
import drainQueue from './drainQueue'
import { v4 as uuidv4 } from 'uuid'
import startHeartbeat from './startHeartbeat'
import { ObserverEmitter } from '../observers'
import { getObserverValue } from './observers'
import * as constants from '../../constants'

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
  public observers: Record<string, Set<ObserverEmitter>>
  public uuid: string
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
    this.uuid = uuidv4()
    this.name = name
    this.type = type
    this.id = id

    this.clients = new Set()
    this.scripts = { batchingEnabled: {}, sha: {} }
    this.serverIsBusy = false
    this.queueInProgress = false
    this.queue = []
    this.connected = false

    const isSubscriptionManager =
      type === 'subscriptionManager' &&
      process.env.SELVA_SERVER_TYPE !== 'subscriptionManager'

    this.on('connect', () => {
      if (!this.connected) {
        console.log('client connected', name)
        this.connected = true
        drainQueue(this)
        if (isSubscriptionManager) {
          startHeartbeat(this)
          // resend subs here?
        }
      }
    })
    this.on('disconnect', () => {
      this.connected = false
      this.queueInProgress = false
      clearTimeout(this.heartbeatTimout)
    })
    this.subscriber = createRedisClient(this, host, port, 'subscriber')
    this.publisher = createRedisClient(this, host, port, 'publisher')

    if (isSubscriptionManager) {
      this.observers = {}
      this.subscriber.on('message', (channel, msg) => {
        if (channel.startsWith(constants.LOG)) {
          // TODO: use log()
          console.log(msg)
        } else if (this.observers[channel]) {
          getObserverValue(this, channel)
        }
      })
    }
  }
}

const clients: Map<string, Client> = new Map()

// sharing on or just putting a seperate on per subscription and handling it from somewhere else?

const createClient = (descriptor: ServerDescriptor): Client => {
  const { type, name, port, host } = descriptor
  const id = `${host}:${port}`
  const client: Client = new Client({
    id,
    name,
    type,
    port,
    host
  })
  return client
}

// const destroyClient = () => {
//   // remove hearthbeat
// }
// export function removeRedisSelvaClient(
//   client: Client,
//   selvaRedisClient: RedisSelvaClient
// ) {
//   // if zero remove the client
// }
// export function addRedisSelvaClient(
//   client: Client,
//   selvaRedisClient: RedisSelvaClient
// ) {
//   // add to a client
// }

export function getClient(
  selvaRedisClient: RedisSelvaClient,
  descriptor: ServerDescriptor
) {
  const { type, port, host } = descriptor
  const id = host + ':' + port
  let client = clients.get(id)
  if (!client) {
    client = createClient(descriptor)
    clients.set(id, client)
    client.subscriber.subscribe(
      `${constants.LOG}:${selvaRedisClient.selvaClient.uuid}`
    )
  }
  if (type === 'origin' || type === 'replica') {
    loadScripts(client)
  }
  // think a bit more about this
  // addRedisSelvaClient(client, selvaRedisClient)
  return client
}

// RESEND SUBS ON RECONNECT
// REMOVE SUBS SET

export function addCommandToQueue(client: Client, redisCommand: RedisCommand) {
  client.queue.push(redisCommand)
  if (!client.queueInProgress) {
    drainQueue(client)
  }
}
