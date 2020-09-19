import { SelvaClient } from '..'
import generateSubscriptionId from './generateSubscriptionId'
import { ObservableOptions } from './types'
import { v4 as uuidv4 } from 'uuid'

import { GetOptions } from '../get'
import { createConnection, Connection } from '../connection'

import {
  NEW_SUBSCRIPTION,
  SUBSCRIPTIONS,
  REMOVE_SUBSCRIPTION,
  HEARTBEAT,
  CLIENTS,
  CACHE
} from '../constants'

import parseError from './parseError'
import { wait } from '../util'

var observableIds = 0
const HEARTBEAT_TIMER = 1e3

type UpdateCallback = (value: any, checksum?: string, diff?: any) => void

// write // remove
// needs to check if they exist on connection

export class Observable {
  public connection: Connection

  constructor(
    options: ObservableOptions,
    selvaClient: SelvaClient,
    uuid?: string
  ) {
    // so this is a bit weird scince we dont do re-use over selva clients for now
    // but this optimizes for diconnecting clients
    // and scince we are going to do more stuff with markers / balancing etc i think this is better
    this.clientUuid = uuidv4()
    this.selvaClient = selvaClient

    if (!this.selvaClient.observables) {
      this.selvaClient.observables = new Map()
    }

    if (!uuid) {
      uuid = generateSubscriptionId(options)
    }

    this.selvaClient.observables.set(uuid, this)

    this.uuid = uuid

    this.selvaId = String('o' + ++observableIds)

    if (options.type === 'get') {
      if (options.cache === undefined) {
        this.useCache = true
      } else {
        this.useCache = options.cache
      }

      if (options.maxMemory === undefined) {
        this.maxMemory = 1e6 // 1 mb as max cache size
      } else {
        this.maxMemory = options.maxMemory
      }

      this.getOptions = options.options
    } else {
      console.log('different type of observable', options)
    }
  }

  public getOptions: GetOptions

  public subscriptionMoved() {
    console.log('this subscription moved to another server! (potentialy')
  }

  public isDestroyed: boolean

  public selvaClient: SelvaClient

  public id: string

  public uuid: string // hash of getoptions

  public clientUuid: string // this will be written on the subs manager fuck it!

  public clients: Set<SelvaClient> = new Set()

  public selvaId: string

  public checksum: string

  public cache: any // last received data

  public maxMemory: number //= 1000000 // 1MB

  public useCache: boolean

  public heartbeatTimout: NodeJS.Timeout

  public startSubscriptionHeartbeat() {
    clearTimeout(this.heartbeatTimout)
    const setHeartbeat = () => {
      if (this.connection) {
        if (this.connection.connected) {
          this.connection.command({
            id: this.selvaId,
            command: 'hset',
            args: [CLIENTS, this.clientUuid, Date.now()]
          })
          this.connection.command({
            command: 'publish',
            id: this.selvaId,
            args: [
              HEARTBEAT,
              JSON.stringify({
                client: this.clientUuid,
                ts: Date.now()
              })
            ]
          })
        }
        this.heartbeatTimout = setTimeout(setHeartbeat, HEARTBEAT_TIMER)
      }
    }
    setHeartbeat()
  }

  public hardDisconnect() {
    console.log('hdc on obs bitch', this.uuid, this.selvaId, this.clientUuid)

    // cleaer timer
    delete this.connection
    clearTimeout(this.heartbeatTimout)
  }

  public listeners: UpdateCallback[] = []

  public errorListeners: ((err: Error) => void)[]

  public completeListeners: ((x?: any) => void)[]

  public isStarted: boolean = false

  public subsCounter: number = 0

  public emitUpdate(value: any, checksum?: string, diff?: any) {
    this.listeners.forEach(fn => fn(value, checksum, diff))
  }

  public emitError(err: Error) {
    if (this.errorListeners) {
      this.errorListeners.forEach(fn => fn(err))
    }
  }

  public emitComplete(x?: any) {
    if (this.completeListeners) {
      this.completeListeners.forEach(fn => fn(x))
    }
  }

  public subscribe(
    // needs an iff for the type of things
    // how to do?
    // TODO: make this type based on the Observable options (type schema or type get for now)
    onNext: UpdateCallback,
    onError?: (err: Error) => void,
    onComplete?: (x?: any) => void
  ) {
    if (this.isDestroyed) {
      console.warn('Observable is allready destroyed!', this.uuid)
      return
    }

    this.subsCounter++

    this.listeners.push(onNext)

    if (onError) {
      if (!this.errorListeners) {
        this.errorListeners = []
      }
      this.errorListeners.push(onError)
    }

    if (onComplete) {
      if (!this.completeListeners) {
        this.completeListeners = []
      }
      this.completeListeners.push(onComplete)
    }

    if (!this.isStarted) {
      this.start()
    } else {
      this.geValueSingleListener(onNext, onError)
    }
  }

  public unsubscribe() {
    if (this.isDestroyed) {
      console.warn('Observable is allready destroyed!', this.uuid)
      return
    }
    this.subsCounter--
    if (this.subsCounter === 0) {
      this.destroy()
    }
  }

  public geValueSingleListener(
    onNext: UpdateCallback,
    onError?: (err: Error) => void
  ) {
    console.log('go')
    if (this.connection) {
      console.log('make time do')
      const channel = this.uuid
      this.connection.command({
        command: 'hmget',
        id: this.selvaId,
        args: [CACHE, channel, channel + '_version'],
        resolve: ([data, version]) => {
          if (data) {
            const obj = JSON.parse(data)
            // obj.version = version
            if (obj.payload && obj.payload.___$error___) {
              if (onError) {
                onError(parseError(obj))
              }
            } else {
              // obj.payload
              onNext(obj.payload, version)
            }
          } else {
            // maybe not send this
            console.log('no datax...')
            onNext(data)
          }
        },
        reject: onError
      })
    }
  }

  public getValue() {
    // then store diff + last diff version
    // so you first do a check here
    if (this.connection) {
      const channel = this.uuid
      this.connection.command({
        command: 'hmget',
        id: this.selvaId,
        args: [CACHE, channel, channel + '_version'],
        resolve: ([data, version]) => {
          if (data) {
            const obj = JSON.parse(data)
            // obj.version = version
            if (obj.payload && obj.payload.___$error___) {
              this.emitError(parseError(obj))
            } else {
              // obj.payload
              this.emitUpdate(obj.payload, version)
            }
          } else {
            // maybe not send this
            console.log('no datax...')
            this.emitUpdate(data, version)
          }
        },
        reject: err => this.emitError(err)
      })
    }
  }

  public async start() {
    if (this.isStarted) {
      console.error('observable allrdy started')
      return
    }

    if (this.connection) {
      console.error(
        'STARTING OBSERVABLE BUT ALLREADY HAVE A CONNECTION WRONG!!!'
      )
      return
    }

    this.isStarted = true

    // to start selecting on process next tick
    // is 1 second maybe too much?
    // keep a timer with in the same tick for selecting subs managers - use this to determine delay
    // await wait(~~(Math.random() * 1000))

    const channel = this.uuid
    const getOptions = this.getOptions
    const server = await this.selvaClient.getServer(
      {
        type: 'subscriptionManager'
      },
      { subscription: channel }
    )
    const connection = (this.connection = createConnection(server))
    const id = this.selvaId
    // yes and then you can handle it yourself also easy to unload things in the q
    connection.attachClient(this)
    connection.command({
      command: 'hsetnx',
      args: [SUBSCRIPTIONS, channel, JSON.stringify(getOptions)],
      id
    })
    connection.command({
      command: 'sadd',
      args: [channel, this.clientUuid],
      id
    })
    connection.command({
      command: 'publish',
      args: [
        NEW_SUBSCRIPTION,
        JSON.stringify({ client: this.clientUuid, channel })
      ],
      id
    })
    // need to start hb
    // also need to get initial value!
    connection.addRemoteListener('message', (incomingChannel, msg) => {
      if (channel === incomingChannel) {
        // msg is checksum
        // will also add diff maybe? or store the last diff?
        console.log('Incoming msg for observable')
      }
    })
    connection.subscribe(channel, id)

    this.startSubscriptionHeartbeat()

    // has to be a bit different!
    console.log('get dat value')
    this.getValue()
  }

  public destroy() {
    if (this.isDestroyed) {
      console.warn('Observable is allready destroyed!', this.uuid)
      return
    }
    clearTimeout(this.heartbeatTimout)

    // need to stop hb

    console.log('destroy obs')

    this.isDestroyed = true

    this.listeners = []
    delete this.errorListeners
    delete this.completeListeners

    // when destroy do this
    const channel = this.uuid
    const id = this.selvaId
    const connection = this.connection
    if (connection) {
      const selvaClientId = this.selvaClient.selvaId
      // use selvaId probably
      connection.unsubscribe(channel, id)

      if (connection.removeClient(this)) {
        connection.removeConnectionState(connection.getConnectionState(id))
      }

      // this is to close so it sues the selva id
      connection.command({
        command: 'srem',
        args: [channel, this.clientUuid],
        id: selvaClientId
      })
      connection.command({
        command: 'publish',
        args: [
          REMOVE_SUBSCRIPTION,
          JSON.stringify({ client: this.clientUuid, channel })
        ],
        id: selvaClientId
      })

      delete this.connection
    }

    this.selvaClient.observables.delete(this.uuid)
  }
}

export const createObservable = (
  options: ObservableOptions,
  selvaClient: SelvaClient
) => {
  const uuid = generateSubscriptionId(options)
  let observable = selvaClient.observables && selvaClient.observables.get(uuid)
  if (!observable) {
    observable = new Observable(options, selvaClient, uuid)
  }
  return observable
}
