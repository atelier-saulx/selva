import { SelvaClient } from '..'
import generateSubscriptionId from './generateSubscriptionId'
import { ObservableOptions } from './types'

var observableIds = 0

type UpdateCallback = (value: any, checksum?: string, diff?: any) => void

export class Observable {
  constructor(
    options: ObservableOptions,
    selvaClient: SelvaClient,
    uuid?: string
  ) {
    this.selvaClient = selvaClient

    if (!this.selvaClient.observables) {
      this.selvaClient.observables = new Map()
    }

    if (!uuid) {
      uuid = generateSubscriptionId(options)
    }

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
    }

    this.selvaClient.observables.set(uuid, this)

    this.uuid = uuid
    this.selvaId = String('o' + ++observableIds)
  }

  public subscriptionMoved() {
    console.log('this subscription moved to another server! (potentialy')
  }

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

  public hardDisconnect() {
    console.log('hdc on obs bitch')
  }

  public listeners: UpdateCallback[] = []

  public errorListeners: ((err: Error) => void)[]

  public completeListeners: ((x?: any) => void)[]

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


  public destroy() {
    if (this.isDestroyed) {
      console.warn('Observable is allready destroyed!', this.uuid)
      return
    }

    console.log('destroy obs')

    this.isDestroyed = true

    this.listeners = []
    delete this.errorListeners
    delete this.completeListeners

    this.selvaClient.observables.delete(this.uuid)
  }
}

export const createObservable = (
  options: ObservableOptions,
  selvaClient: SelvaClient
) => {
  const uuid = generateSubscriptionId(options)
  let observable = selvaClient.observables.get(uuid)
  if (!observable) {
    observable = new Observable(options, selvaClient, uuid)
  }
  return observable
}
