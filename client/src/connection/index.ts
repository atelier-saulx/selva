import { RedisCommand, SelvaClient } from '..'
import { ServerDescriptor } from '../types'
import { v4 as uuidv4 } from 'uuid'
import drainQueue from './drainQueue'
import { loadScripts } from './scripts'
import startRedisClient from './startRedisClient'
import { RedisClient } from 'redis'
import { Callback } from '../redis/types'
import { serverId, isEmptyObject } from '../util'
import { Observable } from '../observable'
import { CLIENTS, HEARTBEAT, STOP_HEARTBEAT, LOG } from '../constants'
import chalk from 'chalk'

const CLIENT_HEARTBEAT_TIMER = 1e3

const connections: Map<string, Connection> = new Map()

type ConnectionState = {
  id?: string
  isEmpty?: boolean
  queue: RedisCommand[]
  pSubscribes: string[]
  subscribes: string[]
  listeners: [string, (x: any) => {}][]
  connectionListeners: [string, (x: any) => {}][]
}

class Connection {
  public subscriber: RedisClient

  public publisher: RedisClient

  public uuid: string

  // uuid  // counter // if counter is zero remove it // if it higher add it
  // maybe just check the redis server fuck it
  // public selvaSubscriptions: {
  //   [key: string]: number
  // }

  public serverDescriptor: ServerDescriptor

  public clients: Set<SelvaClient | Observable> = new Set()

  public subscriptions: { [key: string]: { [key: string]: number } }

  // channel / id
  public psubscriptions: { [key: string]: { [key: string]: number } }

  // listener // id
  public redisListeners: { [key: string]: { [key: string]: Set<Callback> } }

  public listeners: { [key: string]: { [key: string]: Set<Callback> } }

  public hardDisconnect() {
    if (!this.isDestroyed) {
      this.emit('hard-disconnect')
      if (!this.isDestroyed) {
        this.clients.forEach((s) => {
          s.hardDisconnect(this)
        })
        this.destroy()
      }
    }
  }

  public attachClient(client: SelvaClient | Observable) {
    if (
      client instanceof SelvaClient &&
      client.loglevel &&
      !this.clients.has(client)
    ) {
      // @ts-ignore
      const log = `${LOG}:${client.uuid}`

      this.subscribe(log, client.selvaId)
      this.addRemoteListener(
        'message',
        (v, m) => {
          if (v === log) {
            const { msg } = JSON.parse(m)
            console.info(chalk.blue(`lua`), msg)
          }
        },
        client.selvaId
      )
    }

    this.clients.add(client)
  }

  public removeClient(client: SelvaClient | Observable): boolean {
    const hasClient = this.clients.has(client)
    if (hasClient) {
      this.clients.delete(client)
      if (client instanceof Observable) {
        if (this.clientHb[client.selvaClient.uuid]) {
          this.stopClientHb(client.selvaClient.uuid, client.selvaClient.selvaId)
        }
      } else {
        if (this.clientHb[client.uuid]) {
          this.stopClientHb(client.uuid, client.selvaId)
        }
      }
    }
    if (this.clients.size === 0) {
      this.destroyIfIdle()
    }
    return hasClient
  }

  public queue: RedisCommand[]

  public queueBeingDrained: RedisCommand[]

  public connected: boolean = false

  public clientsConnected: { subscriber: boolean; publisher: boolean } = {
    subscriber: false,
    publisher: false,
  }

  // dont add this to state keep it simple
  public clientHb: {
    [uuid: string]: {
      counter: number
      timer: NodeJS.Timeout
    }
  } = {}

  public stopClientHb(uuid: string, id: string) {
    const s = this.clientHb[uuid]
    if (s) {
      s.counter--
      if (s.counter === 0) {
        clearTimeout(s.timer)
        delete this.clientHb[uuid]
        this.command({
          id: id,
          command: 'hdel',
          args: [CLIENTS, uuid],
        })
        this.command({
          command: 'publish',
          id: id,
          args: [STOP_HEARTBEAT, uuid],
        })
      }
    }
  }

  public startClientHb(uuid: string, id: string) {
    if (this.clientHb[uuid]) {
      this.clientHb[uuid].counter++
      return
    }
    this.clientHb[uuid] = { counter: 0, timer: null }
    this.clientHb[uuid].counter++
    clearTimeout(this.clientHb[uuid].timer)
    const setHeartbeat = () => {
      if (this.connected) {
        this.command({
          id: id,
          command: 'hset',
          args: [CLIENTS, uuid, Date.now()],
        })
        this.command({
          command: 'publish',
          id: id,
          args: [
            HEARTBEAT,
            JSON.stringify({
              client: uuid,
              ts: Date.now(),
            }),
          ],
        })
      }
      this.clientHb[uuid].timer = setTimeout(
        setHeartbeat,
        CLIENT_HEARTBEAT_TIMER
      )
    }
    this.command({
      id: id,
      command: 'hset',
      args: [CLIENTS, uuid, Date.now()],
    })
    setHeartbeat()
  }

  public isDestroyed: boolean = false

  public serverIsBusy: boolean = false

  public queueInProgress: boolean = false

  public selvaSubscriptionsActive: boolean = false

  public genericSubscribe(
    type: 'subscriptions' | 'psubscriptions',
    method: 'subscribe' | 'psubscribe',
    channel: string,
    id: string = ''
  ) {
    if (!this[type]) {
      this[type] = {}
    }
    if (!this[type][channel]) {
      this[type][channel] = {}
      this.subscriber[method](channel)
      this.addActive()
    }
    if (this[type][channel][id] === undefined) {
      this[type][channel][id] = 0
    }
    this[type][channel][id]++
  }

  public genericUnsubscribe(
    type: 'subscriptions' | 'psubscriptions',
    method: 'punsubscribe' | 'unsubscribe',
    channel: string,
    id: string = ''
  ) {
    if (this[type] && this[type][channel] && this[type][channel][id]) {
      this[type][channel][id]--
      if (this[type][channel][id] === 0) {
        delete this[type][channel][id]
      }
      if (Object.keys(this[type][channel]).length === 0) {
        delete this[type][channel]
        this.subscriber[method](channel)
        this.removeActive()
      }
    }
  }

  public subscribe(channel: string, id: string = '') {
    if (!id) {
      console.log('NO ID PROVIDED', channel, id, new Error().stack)
    }
    this.genericSubscribe('subscriptions', 'subscribe', channel, id)
  }

  public unsubscribe(channel: string, id?: string) {
    this.genericUnsubscribe('subscriptions', 'unsubscribe', channel, id)
  }

  public psubscribe(channel: string, id?: string) {
    this.genericSubscribe('psubscriptions', 'psubscribe', channel, id)
  }

  public punsubscribe(channel: string, id?: string) {
    this.genericUnsubscribe('psubscriptions', 'punsubscribe', channel, id)
  }

  public command(command: RedisCommand) {
    this.queue.push(command)
    if (!this.queueInProgress && this.connected) {
      drainQueue(this)
    }
    // only starts destroying on connect
  }

  public removeRemoteListener(event: string, cb?: Callback, id: string = '') {
    const listeners = this.redisListeners
    if (listeners && listeners[event] && listeners[event][id]) {
      if (cb) {
        listeners[event][id].delete(cb)
        if (!listeners[event][id].size) {
          this.subscriber.removeListener(event, cb)
          delete listeners[event][id]
          if (isEmptyObject(listeners[event])) {
            delete listeners[event]
          }
        }
      } else {
        listeners[event][id].forEach((cb) => {
          this.subscriber.removeListener(event, cb)
        })
        delete listeners[event][id]
        if (isEmptyObject(listeners[event])) {
          delete listeners[event]
        }
      }
    }
  }

  public emit(event: string, payload?: any) {
    const listeners = this.listeners[event]
    if (listeners) {
      for (let id in listeners) {
        listeners[id].forEach((cb) => {
          cb(payload)
        })
      }
    }
  }

  public removeListener(event: string, cb?: Callback, id: string = '') {
    const listeners = this.listeners
    if (listeners && listeners[event] && listeners[event][id]) {
      if (cb) {
        listeners[event][id].delete(cb)
        if (!listeners[event][id].size) {
          this.subscriber.removeListener(event, cb)
          delete listeners[event][id]
          if (isEmptyObject(listeners[event])) {
            delete listeners[event]
          }
        }
      } else {
        delete listeners[event][id]
        if (isEmptyObject(listeners[event])) {
          delete listeners[event]
        }
      }
    }
  }

  public on(event: string, cb: Callback, id: string = '') {
    this.addListener(event, cb, id)
  }

  public addListener(event: string, cb: Callback, id: string = '') {
    let listeners = this.listeners
    if (!listeners) {
      listeners = this.listeners = {}
    }
    if (!listeners[event]) {
      listeners[event] = {}
    }
    if (!listeners[event][id]) {
      listeners[event][id] = new Set()
    }
    listeners[event][id].add(cb)
  }

  public removeAllListeners(id: string = '') {
    // make it gone
    if (id) {
      for (const event in this.listeners) {
        if (this.listeners[event][id]) {
          delete this.listeners[event][id]
          if (isEmptyObject(this.listeners[event])) {
            delete this.listeners[event]
          }
        }
      }
    } else {
      this.listeners = {}
    }
  }

  public addRemoteListener(event: string, cb?: Callback, id: string = '') {
    let listeners = this.redisListeners
    if (!listeners) {
      listeners = this.redisListeners = {}
    }
    if (!listeners[event]) {
      listeners[event] = {}
    }
    if (!listeners[event][id]) {
      listeners[event][id] = new Set()
    }
    listeners[event][id].add(cb)
    this.subscriber.on(event, cb)
  }

  public applyConnectionState(state: ConnectionState) {
    if (!state.isEmpty) {
      if (state.pSubscribes.length) {
        for (let i = 0; i < state.pSubscribes.length; i++) {
          const sub = state.pSubscribes[i]
          this.psubscribe(sub, state.id)
        }
      }
      if (state.subscribes.length) {
        for (let i = 0; i < state.subscribes.length; i++) {
          const sub = state.subscribes[i]
          this.subscribe(sub, state.id)
        }
      }
      if (state.listeners.length) {
        for (let i = 0; i < state.listeners.length; i++) {
          const [event, cb] = state.listeners[i]
          this.addRemoteListener(event, cb, state.id)
        }
      }
      if (state.connectionListeners.length) {
        for (let i = 0; i < state.connectionListeners.length; i++) {
          const [event, cb] = state.connectionListeners[i]
          this.addListener(event, cb, state.id)
        }
      }
      if (state.queue.length) {
        for (let i = 0; i < state.queue.length; i++) {
          this.command(state.queue[i])
        }
      }
    }
  }

  public removeConnectionState(state: ConnectionState) {
    if (!state.isEmpty) {
      const id = state.id
      if (state.listeners.length) {
        for (let i = 0; i < state.listeners.length; i++) {
          const [event, callback] = state.listeners[i]
          this.removeRemoteListener(event, callback, id)
        }
      }
      if (state.queue.length) {
        for (let i = 0; i < state.queue.length; i++) {
          const q = state.queue[i]
          let f
          for (let j = 0; j < this.queue.length; j++) {
            if (this.queue[j] === q) {
              f = true
              this.queue.splice(j, 1)
              break
            }
          }
          if (!f) {
            for (let j = 0; j < this.queue.length; j++) {
              if (this.queueBeingDrained[j] === q) {
                f = true
                this.queueBeingDrained.splice(j, 1)
                break
              }
            }
          }
        }
      }
      if (state.subscribes.length) {
        for (let i = 0; i < state.subscribes.length; i++) {
          this.unsubscribe(state.subscribes[i], id)
        }
      }
      if (state.pSubscribes.length) {
        for (let i = 0; i < state.pSubscribes.length; i++) {
          this.punsubscribe(state.subscribes[i], id)
        }
      }
      if (state.connectionListeners.length) {
        this.removeAllListeners(id)
      }
    }
    this.destroyIfIdle()
  }

  public getConnectionState(id?: string): ConnectionState {
    // no id needs to mean all :/
    // different behaviour then empty string
    // nice to know the id
    const state = {
      isEmpty: true,
      id,
      queue: [],
      pSubscribes: [],
      subscribes: [],
      listeners: [],
      connectionListeners: [],
      selvaSubscriptions: [],
    }

    // and do this also
    if (id === undefined) {
      // empty string is also a target
      // take the quue
      // do this later
    } else {
      for (const channel in this.psubscriptions) {
        if (id in this.psubscriptions[channel]) {
          state.isEmpty = false
          state.pSubscribes.push(channel)
        }
      }
      for (const channel in this.subscriptions) {
        if (id in this.subscriptions[channel]) {
          state.isEmpty = false
          state.subscribes.push(channel)
        }
      }

      for (const event in this.redisListeners) {
        if (id in this.redisListeners[event]) {
          this.redisListeners[event][id].forEach((cb) => {
            state.isEmpty = false
            state.listeners.push([event, cb])
          })
        }
      }

      for (const event in this.listeners) {
        if (id in this.listeners[event]) {
          this.listeners[event][id].forEach((cb) => {
            state.isEmpty = false
            state.connectionListeners.push([event, cb])
          })
        }
      }

      if (this.queueBeingDrained) {
        const q = this.queueBeingDrained.filter((command) => command.id === id)
        if (q.length) {
          state.isEmpty = false
          state.queue = q
        }
      }

      if (this.queue) {
        const q = this.queue.filter((command) => command.id === id)
        if (q.length) {
          state.isEmpty = false
          state.queue = [...state.queue, ...q]
        }
      }
    }

    return state
  }

  public isDc: boolean

  public startClientTimer: NodeJS.Timeout = null

  public serverHeartbeatTimer: NodeJS.Timeout = null

  public activeCounter = 0
  destroyTimer: NodeJS.Timeout = null

  // add destroy timer on create?
  public addActive() {
    this.activeCounter++
    if (this.destroyTimer) {
      clearTimeout(this.destroyTimer)
    }
    this.destroyTimer = null
  }

  public destroyIfIdle() {
    // maybe have to say after a dc just throw it away
    // dont know if connectd is rly nessecary for thi
    if (
      this.activeCounter === 0 &&
      !this.destroyTimer &&
      (this.connected || this.isDc) &&
      !this.isDestroyed
    ) {
      this.destroyTimer = setTimeout(() => {
        this.destroyTimer = null
        // console.log(`🐰 Destroy connection from idle ${}`, this.serverDescriptor)
        this.destroy()
      }, 3000)
    }
  }

  public removeActive() {
    this.activeCounter--
    this.destroyIfIdle()
  }

  public destroy() {
    // remove this log later...
    // console.log('Destroy connection', serverId(this.serverDescriptor))

    if (this.isDestroyed) {
      console.warn('Allready destroyed connection', this.serverDescriptor)
      return
    }

    this.clients = new Set()

    this.isDestroyed = true

    this.subscriber.removeAllListeners()
    this.publisher.removeAllListeners()

    this.subscriber.on('error', () => {})
    this.publisher.on('error', () => {})

    this.subscriber.quit()
    this.publisher.quit()

    if (this.destroyTimer) {
      clearTimeout(this.destroyTimer)
    }
    this.destroyTimer = null

    if (this.serverHeartbeatTimer) {
      clearTimeout(this.serverHeartbeatTimer)
    }
    this.serverHeartbeatTimer = null

    if (this.startClientTimer) {
      clearTimeout(this.startClientTimer)
    }

    for (const k in this.clientHb) {
      // does not send a remove client event scine we want those to stay persistent trough a hdc
      clearTimeout(this.clientHb[k].timer)
      delete this.clientHb[k]
    }

    this.startClientTimer = null

    connections.delete(serverId(this.serverDescriptor))

    // destroy if counter is zero
    if (this.selvaSubscriptionsActive) {
      console.log(
        'need to remove subs listeners for hearthbeat, and need to remove message listener'
      )
    }

    this.emit('close')

    // remove all listeners -- pretty dangerous
    this.removeAllListeners()
  }

  constructor(serverDescriptor: ServerDescriptor) {
    this.uuid = uuidv4()

    this.serverDescriptor = serverDescriptor

    this.queue = []
    this.queueBeingDrained = []

    // here we add the retry strategies
    // - start timeout
    // - max retries
    // - server timeout subscription
    // - hard-disconnect (from info)

    // make this in a function (for retries etc)
    startRedisClient(this)

    const stringId = serverId(serverDescriptor)

    if (connections.get(stringId)) {
      console.warn('⚠️  Connection allready exists! ', stringId)
    }

    connections.set(stringId, this)

    if (
      serverDescriptor.type === 'origin' ||
      serverDescriptor.type === 'replica'
    ) {
      loadScripts(this)
    }

    // connection connect
    this.on(
      'connect',
      () => {
        // this is prob a good place...
        this.destroyIfIdle()
        if (this.queue.length) {
          drainQueue(this)
        }
      },
      'connection'
    )
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

// for logging
