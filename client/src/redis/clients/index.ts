import { RedisClient } from 'redis'
import { RedisCommand, Callback } from '../types'
import RedisSelvaClient from '../'
import './redisSearch'
import { ServerType, ServerDescriptor, LogEntry } from '../../types'
import { EventEmitter } from 'events'
import createRedisClient from './createRedisClient'
import { loadScripts } from './scripts'
import drainQueue from './drainQueue'
import { v4 as uuidv4 } from 'uuid'
import startHeartbeat from './startHeartbeat'
import { ObserverEmitter } from '../observers'
import { getObserverValue, sendObserver } from './observers'
import getServerDescriptor from '../getServerDescriptor'
import * as constants from '../../constants'
import handleListenerClient from './handleListenerClient'

type ClientOpts = {
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

const reconnectClient = (client, retry: number = 0) => {
  const { type, name } = client
  const clients = [...client.clients.values()]
  const aSelvaClient = clients[0]

  const q = [...client.queue, ...client.queueBeingDrained]

  getServerDescriptor(aSelvaClient, {
    type,
    name
  }).then(descriptor => {
    console.log('reconnecting to ', descriptor.port, retry)

    if (descriptor.host + ':' + descriptor.port === client.id && retry < 5) {
      console.log('TRYING TO RECONNECT TO THE SAME WAIT A BIT')
      setTimeout(() => {
        reconnectClient(client, retry + 1)
      }, 1e3)
      return
    }

    let newClient
    clients.forEach(selvaClient => {
      setTimeout(() => {
        selvaClient.selvaClient.emit('reconnect', descriptor)
      }, 500)
      newClient = getClient(selvaClient, descriptor)
    })

    for (let event in client.redisListeners) {
      console.log(
        're-applying listeners for',
        event,
        client.redisListeners[event].length
      )
      client.redisListeners[event].forEach(callback => {
        handleListenerClient(newClient, 'on', event, callback)
      })
    }

    newClient.redisSubscriptions = client.redisSubscriptions

    q.forEach(command => {
      console.log('reapply command', command.command, command.args[0])
      addCommandToQueue(newClient, command)
    })

    destroyClient(client)
  })
}

export class Client extends EventEmitter {
  public subscriber: RedisClient
  public publisher: RedisClient
  public redisSubscriptions: {
    psubscribe: Record<string, true>
    subscribe: Record<string, true>
  } = {
    psubscribe: {},
    subscribe: {}
  }

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

    this.on('disconnect', () => {
      console.log('make dc come on')
    })

    this.on('hard-disconnect', () => {
      // find different server for it

      console.log(
        'hard dc - prob need to reconnect to somethign new',
        port,
        host,
        type,
        name
      )

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
          this.subscriber.subscribe(key)
        }

        for (const key in this.redisSubscriptions.psubscribe) {
          this.subscriber.psubscribe(key)
        }

        if (isSubscriptionManager) {
          startHeartbeat(this)
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

const destroyClient = (client: Client) => {
  client.queue = []
  client.clients = new Set()
  clients.delete(client.id)
  client.observers = {}
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
    client.subscriber.subscribe(
      `${constants.LOG}:${selvaRedisClient.selvaClient.uuid}`
    )
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
