import { SelvaClient } from '..'
import generateSubscriptionId from './generateSubscriptionId'
import { ObservableOptions } from './types'
import { GetOptions } from '../get'
import { createConnection, Connection } from '../connection'
import {
  NEW_SUBSCRIPTION,
  SUBSCRIPTIONS,
  REMOVE_SUBSCRIPTION,
  REGISTRY_MOVE_SUBSCRIPTION,
  CACHE
} from '../constants'
import parseError from './parseError'
import { ServerSelector } from '../types'
import chalk, { keyword } from 'chalk'

var observableIds = 0

type UpdateCallback = (value: any, checksum?: string, diff?: any) => void

// write // remove
// needs to check if they exist on connection

export class Observable {
  public connection: Connection
  public options: ObservableOptions

  constructor(
    options: ObservableOptions,
    selvaClient: SelvaClient,
    uuid?: string
  ) {
    // so this is a bit weird scince we dont do re-use over selva clients for now
    // but this optimizes for diconnecting clients
    // and scince we are going to do more stuff with markers / balancing etc i think this is better
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
      this.options = options

      console.log('different type of observable', options)
    }
  }

  public getOptions: GetOptions

  public isDestroyed: boolean

  public selvaClient: SelvaClient

  public id: string

  public uuid: string // hash of getoptions

  public clients: Set<SelvaClient> = new Set()

  public selvaId: string

  public checksum: string

  public cache: any // last received data

  public maxMemory: number //= 1000000 // 1MB

  public useCache: boolean

  public async hardDisconnect() {
    // just call start again
    this.isStarted = false
    const prevServer = `${this.connection.serverDescriptor.host}:${this.connection.serverDescriptor.port}`
    delete this.connection
    console.log(
      chalk.yellow(
        `Hard disconnection event on observable ${this.uuid} ${prevServer}`
      )
    )
    await this.start()
    console.log(
      chalk.gray(
        `Successfully restarted observable ${this.uuid} after hard disconnect connect to ${this.connection.serverDescriptor.host}:${this.connection.serverDescriptor.port}`
      )
    )
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
      console.warn(
        chalk.yellow(
          `Trying to subscribe to an observable that is already destroyed`
        ),
        this.getOptions || this.options
      )
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
      console.warn(
        chalk.yellow(
          `Trying to unsubscribe to an observable that is already destroyed ${this.uuid}`
        )
      )
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
              if (onError) {
                onError(parseError(obj))
              }
            } else {
              // obj.payload
              onNext(obj.payload, version)
            }
          } else {
            // maybe not send this
            // console.log('no datax...')
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
            // console.log('no datax...')
            this.emitUpdate(data, version)
          }
        },
        reject: err => this.emitError(err)
      })
    }
  }

  public async moveToServer(selector: ServerSelector) {
    if (this.isDestroyed) {
      return
    }
    if (this.connection.removeClient(this)) {
      this.connection.removeConnectionState(
        this.connection.getConnectionState(this.id)
      )
    }
    delete this.connection
    this.isStarted = false
    await this.start(selector)
  }

  public async start(selector?: ServerSelector) {
    if (this.isStarted) {
      console.warn(
        chalk.yellow(
          `Trying to start an observable that is already started ${this.uuid}`
        )
      )
      return
    }
    if (this.connection) {
      console.warn(
        chalk.yellow(
          `Trying to start an observable that already has an active connection ${this.uuid}`
        )
      )
      return
    }

    this.isStarted = true

    const channel = this.uuid
    const getOptions = this.getOptions

    // to start selecting on process next tick
    // is 1 second maybe too much?
    // keep a timer with in the same tick for selecting subs managers - use this to determine delay
    // await wait(~~(Math.random() * 1000))
    const server = await this.selvaClient.getServer(
      selector || {
        type: 'subscriptionManager'
      },
      { subscription: channel }
    )
    const connection = (this.connection = createConnection(server))
    const id = this.selvaId

    connection.attachClient(this)
    connection.command({
      command: 'hsetnx',
      args: [SUBSCRIPTIONS, channel, JSON.stringify(getOptions)],
      id
    })
    connection.command({
      command: 'sadd',
      args: [channel, this.selvaClient.uuid],
      id
    })
    connection.command({
      command: 'publish',
      args: [
        NEW_SUBSCRIPTION,
        JSON.stringify({ client: this.selvaClient.uuid, channel })
      ],
      id
    })

    connection.addRemoteListener('message', (incomingChannel, msg) => {
      if (incomingChannel === REGISTRY_MOVE_SUBSCRIPTION) {
        const [moveChannel, newServer] = JSON.parse(msg)
        if (channel === moveChannel) {
          console.info(
            chalk.gray(
              `Receive move command in observable from ${this.connection.serverDescriptor.host}:${this.connection.serverDescriptor.port} to ${newServer} (channel: ${this.uuid})`
            )
          )
          const [host, port] = newServer.split(':')
          this.moveToServer({
            type: 'subscriptionManager',
            host,
            port: Number(port)
          })
        }
      } else if (channel === incomingChannel) {
        // msg is checksum
        // will also add diff maybe? or store the last diff?
        console.log('Incoming msg for observable', msg)
      }
    })

    connection.subscribe(channel, id)
    connection.subscribe(REGISTRY_MOVE_SUBSCRIPTION, id)

    connection.startClientHb(this.selvaClient.uuid, this.selvaClient.selvaId)

    // has to be a bit different!
    this.getValue()
  }

  public destroy() {
    if (this.isDestroyed) {
      console.warn(
        chalk.yellow(
          `Trying to destroy an observable that is already destroyed ${this.uuid}`
        )
      )
      return
    }

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
        args: [channel, this.selvaClient.uuid],
        id: selvaClientId
      })
      connection.command({
        command: 'publish',
        args: [
          REMOVE_SUBSCRIPTION,
          JSON.stringify({ client: this.selvaClient.uuid, channel })
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
