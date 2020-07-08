import { RedisClient } from 'redis'
import { RedisCommand, Callback } from '../types'
import RedisSelvaClient from '../'
import './redisClientExtensions'
import { ServerType, ServerDescriptor, LogEntry } from '../../types'
import { EventEmitter } from 'events'
import createRedisClient from './createRedisClient'
import { loadScripts } from './scripts'
import drainQueue from './drainQueue'
import { v4 as uuidv4 } from 'uuid'
import startSubscriptionHeartbeat from './startSubscriptionHeartbeat'
import { ObserverEmitter } from '../observers'
import { getObserverValue, sendObserver } from './observers'
import * as constants from '../../constants'
import { SERVER_HEARTBEAT } from '../../constants'
import reconnectClient from './reconnectClient'

export type RedisSubscriptions = {
  psubscribe: Record<string, true>
  subscribe: Record<string, true>
}

export type ClientOpts = {
  name: string
  type: ServerType
  host: string
  port: number
  id: string
}

const addListeners = (client: Client) => {
  const type = client.type
  const isSubscriptionManager = type === 'subscriptionManager'

  if (isSubscriptionManager) {
    client.subscriber.on('message', (channel, msg) => {
      if (client.observers[channel]) {
        getObserverValue(client, channel)
      }
    })
  } else {
    client.subscriber.on('message', (channel, msg) => {
      if (channel.startsWith(constants.LOG)) {
        const log: LogEntry = JSON.parse(msg)
        for (const cl of client.clients) {
          if (cl.selvaClient.uuid === log.clientId) {
            cl.selvaClient.emit('log', { dbName: client.name, log })
          }
        }
      }
    })
  }
}

export class Client extends EventEmitter {
  public subscriber: RedisClient
  public publisher: RedisClient
  public redisSubscriptions: RedisSubscriptions = {
    psubscribe: {},
    subscribe: {}
  }

  public isRemoved: boolean
  public startClientTimer: NodeJS.Timeout
  public serverHeartbeat: NodeJS.Timeout

  // event, listeners
  public redisListeners: Record<string, Callback[]> = {}
  public queue: RedisCommand[]
  public queueInProgress: boolean
  public name: string // for logging
  public type: ServerType // for logs
  public id: string // url:port
  public connected: boolean
  public observers: Record<string, Set<ObserverEmitter>>
  public uuid: string
  public queueBeingDrained: RedisCommand[]
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
    this.queueBeingDrained = []
    this.connected = false

    const isSubscriptionManager = type === 'subscriptionManager'

    this.on('hard-disconnect', () => {
      // find different server for it
      this.emit('disconnect')
      console.log(
        'hard dc - prob need to reconnect to somethign new',
        port,
        host,
        type,
        name
      )

      this.subscriber.removeAllListeners()
      this.publisher.removeAllListeners()
      this.subscriber.quit()
      this.publisher.quit()

      if (type === 'registry') {
        this.subscriber = createRedisClient(this, host, port, 'subscriber')
        this.publisher = createRedisClient(this, host, port, 'publisher')
        addListeners(this)
      } else {
        if (type === 'subscriptionManager') {
          console.log('ok subs manager we can throw this away')
          destroyClient(this)
          // remove sub manager! send it allrdy
        } else {
          reconnectClient(this)
        }
      }
    })

    this.on('connect', () => {
      if (!this.connected) {
        this.connected = true
        drainQueue(this)

        for (const key in this.redisSubscriptions.subscribe) {
          addCommandToQueue(this, { command: 'subscribe', args: [key] })
        }

        for (const key in this.redisSubscriptions.psubscribe) {
          addCommandToQueue(this, { command: 'psubscribe', args: [key] })
        }

        addCommandToQueue(this, {
          command: 'subscribe',
          args: [SERVER_HEARTBEAT]
        })

        if (isSubscriptionManager) {
          startSubscriptionHeartbeat(this)
          for (const channel in this.observers) {
            let sendSubs = false
            this.observers[channel].forEach(obs => {
              if (obs.isSend) {
                if (!sendSubs) {
                  sendObserver(this, channel, obs.getOptions)
                  sendSubs = true
                }
                getObserverValue(this, channel, obs)
              } else {
                sendSubs = true
              }
            })
          }
        }
      }
    })

    this.on('disconnect', () => {
      // on dc we actualy want to re-select if it had a selector!
      this.queue.concat(this.queueBeingDrained)
      this.queueBeingDrained = []
      this.connected = false
      this.queueInProgress = false
      clearTimeout(this.heartbeatTimout)
    })

    this.subscriber = createRedisClient(this, host, port, 'subscriber')
    this.publisher = createRedisClient(this, host, port, 'publisher')

    if (isSubscriptionManager) {
      this.observers = {}
    }

    addListeners(this)
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

export const destroyClient = (client: Client) => {
  client.queue = []
  client.clients = new Set()
  clients.delete(client.id)
  client.observers = {}
  client.isRemoved = true
  client.queueBeingDrained = []
  client.removeAllListeners()
  client.redisListeners = {}
  client.redisSubscriptions = { subscribe: {}, psubscribe: {} }
}

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
    if (type === 'origin' || type === 'replica') {
      loadScripts(client)
    }
  }

  if (!client.clients.has(selvaRedisClient)) {
    addCommandToQueue(client, {
      command: 'subscribe',
      args: [`${constants.LOG}:${selvaRedisClient.selvaClient.uuid}`]
    })

    client.clients.add(selvaRedisClient)
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
