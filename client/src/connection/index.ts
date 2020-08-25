import { EventEmitter } from 'events'
import { RedisCommand } from '..'
import { SERVER_HEARTBEAT } from '../constants'
import { RedisClient } from 'redis'
import { ServerDescriptor } from '../types'
import { v4 as uuidv4 } from 'uuid'

const connections: Map<string, Connection> = new Map()

const serverId = (serverDescriptor: ServerDescriptor) => {
  return serverDescriptor.host + ':' + serverDescriptor.port
}

type ConnectionState = {
  queue: RedisCommand[]
  pSubscribes: string[]
  subscribes: string[]
  listeners: [string, (x: any) => {}][]
}

class Connection extends EventEmitter {
  public subscriber: RedisClient

  public publisher: RedisClient

  public uuid: string

  public serverDescriptor: ServerDescriptor

  public selvaSubscribe() {
    // add hearthbeat if you dont have it
  }

  public selvaUnsubscribe() {
    // if empty (no selva subscriptions stop hearthbeat)
    // and handle activity counter ofc
  }

  public queue: RedisCommand[]

  public queueBeingDrained: RedisCommand[]

  public connected: boolean = false

  public serverIsBusy: boolean = false

  public queueInProgress: boolean = false

  public subscribe() {}

  public unsubscribe() {}

  public psubscribe() {}

  public punsubscribe() {}

  public addCommand(command: RedisCommand) {
    // also handes subsriptions etc
  }

  public applyConnectionState(state: ConnectionState) {}

  public getConnectionState(id?: string): ConnectionState {
    const state = {
      queue: [],
      pSubscribes: [],
      subscribes: [],
      listeners: []
    }

    if (id) {
      // take the quue
    } else {
      // copy it nice
    }

    return state
  }

  public removeRemoteListener() {}
  public addRemoteListener() {}

  // mostly used internaly
  public activeCounter = 0
  destroyTimer: NodeJS.Timeout = null
  public addActive() {
    this.activeCounter++
    if (this.destroyTimer) {
      clearTimeout(this.destroyTimer)
    }
    this.destroyTimer = null
  }
  public removeActive() {
    this.activeCounter--
    if (this.activeCounter === 0 && !this.destroyTimer) {
      this.destroyTimer = setTimeout(() => {
        this.destroy()
      }, 1000)
    }
  }

  public destroy() {
    console.log('destroy connection')
    if (this.destroyTimer) {
      clearTimeout(this.destroyTimer)
    }
    this.destroyTimer = null

    connections.delete(serverId(this.serverDescriptor))

    // destroy if counter is zero
  }

  constructor(serverDescriptor: ServerDescriptor) {
    super()

    this.setMaxListeners(1e5)

    this.uuid = uuidv4()

    this.serverDescriptor = serverDescriptor

    this.queue = []
    this.queueBeingDrained = []

    // here we add the retry strategies

    // we add
    // - start timeout
    // - max retries
    // - server timeout subscription
    // - hard-disconnect (from info)

    this.subscriber = new RedisClient({
      host: serverDescriptor.host,
      port: serverDescriptor.port
    })

    this.publisher = new RedisClient({
      host: serverDescriptor.host,
      port: serverDescriptor.port
    })

    const stringId = serverId(serverDescriptor)

    if (connections.get(stringId)) {
      console.warn('connection allready exists! ', stringId)
    }

    connections.set(stringId, this)
  }
}

const createConnection = (serverDescriptor: ServerDescriptor) => {
  let connection = connections.get(serverId(serverDescriptor))
  if (!connection) {
    connection = new Connection(serverDescriptor)
  }
  return connection
}

export { createConnection, connections, Connection }

// lets start with this one
