import { EventEmitter } from 'events'
import { RedisCommand, SelvaClient } from '..'
import { ServerDescriptor } from '../types'
import { v4 as uuidv4 } from 'uuid'
import drainQueue from './drainQueue'
import { loadScripts } from './scripts'
import SubscriptionEmitter from '../observe/emitter'
import startRedisClient from './startRedisClient'
import { RedisClient } from 'redis'
import { Callback } from '../redis/types'
import { serverId } from '../util'

const connections: Map<string, Connection> = new Map()

// logs are easier to handle just add the id in the command to the other id

type ConnectionState = {
  id?: string
  isEmpty?: boolean
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

  public selvaSubscriptionEmitters: Set<SubscriptionEmitter>

  public selvaClients: Set<SelvaClient> = new Set()

  public subscriptions: { [key: string]: { [key: string]: number } }

  // channel / id
  public psubscriptions: { [key: string]: { [key: string]: number } }

  // listener / id
  public redisListeners: { [key: string]: { [key: string]: Set<Callback> } }

  public hardDisconnect() {
    this.emit('hard-disconnect')
    this.selvaClients.forEach(s => {
      s.hardDisconnect(this)
    })
    this.destroy()
  }

  public attachSelvaClient(selvaClient: SelvaClient) {
    this.selvaClients.add(selvaClient)
  }

  public removeSelvaClient(selvaClient: SelvaClient): boolean {
    const hasClient = this.selvaClients.has(selvaClient)
    if (hasClient) {
      this.selvaClients.delete(selvaClient)
    }
    return hasClient
  }

  public selvaSubscribe() {
    if (!this.selvaSubscriptionsActive) {
      console.log('need to add hearthbeat')
      console.log('need to add message listener')
      // this does not have to go to the state (scince we have the selva subscription itself!)
      this.selvaSubscriptionsActive = true
    }
    console.log('selva subscribe')
    // need to add a counter to the subscription
    // add hearthbeat if you dont have it
    // initializeSubscriptions
    // returns value as well
  }

  public selvaUnsubscribe() {
    // if empty (no selva subscriptions stop hearthbeat)
    // and handle activity counter ofc
    // if counter === 0 remove subs and remove
    // if all counters are zero remove all ( also on destory )
  }

  public queue: RedisCommand[]

  public queueBeingDrained: RedisCommand[]

  public connected: boolean = false

  public clientsConnected: { subscriber: boolean; publisher: boolean } = {
    subscriber: false,
    publisher: false
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
          if (Object.keys(listeners[event]).length === 0) {
            delete listeners[event]
          }
        }
      } else {
        listeners[event][id].forEach(cb => {
          this.subscriber.removeListener(event, cb)
        })
        delete listeners[event][id]
        if (Object.keys(listeners[event]).length === 0) {
          delete listeners[event]
        }
      }
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

          console.log(event, cb)
          this.addRemoteListener(event, cb, state.id)
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
    }
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
      listeners: []
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
          this.redisListeners[event][id].forEach(cb => {
            state.isEmpty = false
            state.listeners.push([event, cb])
          })
        }
      }

      if (this.queueBeingDrained) {
        const q = this.queueBeingDrained.filter(command => command.id === id)1
        if (q.length) {
          state.isEmpty = false
          state.queue = q
        }
      }

      if (this.queue) {
        const q = this.queue.filter(command => command.id === id)
        if (q.length) {
          state.isEmpty = false
          state.queue = [...state.queue, ...q]
        }
      }
    }

    return state
  }

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
    // dont know if connectd is rly nessecary for thi
    if (this.activeCounter === 0 && !this.destroyTimer && this.connected) {
      this.destroyTimer = setTimeout(() => {
        console.log('🐰 Destroy connection from idle')
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
    console.log('Destroy connection', serverId(this.serverDescriptor))

    if (this.isDestroyed) {
      console.warn('Allready destroyed connection', this.serverDescriptor)
      return
    }

    this.selvaClients = new Set()

    this.isDestroyed = true

    this.subscriber.removeAllListeners()
    this.publisher.removeAllListeners()

    this.subscriber.on('error', () => { })
    this.publisher.on('error', () => { })

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
    super()

    this.setMaxListeners(1e5)

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

    this.on('connect', () => {
      // this is prob a good place...
      this.destroyIfIdle()
      if (this.queue.length) {
        drainQueue(this)
      }
    })
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
