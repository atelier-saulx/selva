import { SelvaClient } from '..'
import generateSubscriptionId from './generateSubscriptionId'
import { ObservableOptions } from './types'

var observableIds = 0

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

  public unsubscribe() {}

  public subscribe(
    // needs an iff for the type of things

    // how to do?

    // TODO: make this type based on the Observable options (type schema or type get for now)
    onNext: (x: any) => void,
    onError?: (e: Error) => void,
    onComplete?: () => void
  ) {
    /*
    subscribe(
      onNext: ((x: T) => void) | Observer<T>,
      onError?: (e: Error) => void,
      onComplete?: () => void
    )
    */
    // make this nice observable like
  }

  public destroy() {
    console.log('destroy')
    this.isDestroyed = true

    this.selvaClient.observables.delete(this.uuid)
  }
}

// maybe have to remove the re-use... :/
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
