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

class Connection extends EventEmitter {
  public subscriber: RedisClient

  public publisher: RedisClient

  public uuid: string

  public serverDescriptor: ServerDescriptor

  public selvaSubscribe() {}

  public selvaUnsubscribe() {}

  // better maybe to remove this from the queue? OR everything in the queue
  public subscribe() {}

  public unsubscribe() {}

  public psubscribe() {}

  public punsubscribe() {}

  public addCommand(command: RedisCommand) {
    // also handes subsriptions etc
  }

  public applyConnectionState() {}

  public getConnectionState(id: string) {
    // if !id just get all
    // return something that you can pass into mergeConnection
    // queue from in progress and current queue
    // subscriptions
    // listeners
    // selvaSubscriptions
    // psubscribe
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

    // destory if counter is zero
  }

  constructor(serverDescriptor: ServerDescriptor) {
    super()

    this.setMaxListeners(1e5)

    this.uuid = uuidv4()

    this.serverDescriptor = serverDescriptor
    // here we add the retry strategies

    // we add
    // - start timeout
    // - max retries
    // - server timeout subscription

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
