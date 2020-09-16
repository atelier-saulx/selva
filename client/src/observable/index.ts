import { SelvaClient } from '..'
import generateSubscriptionId from './generateSubscriptionId'
import { ObservableOptions } from './types'
// has state as well

export const observables: Map<string, Observable> = new Map()

var observableIds = 0

// need an 'update' method

// unique ness is the resulting connect options of a selva client

// give registry a unique id?

// or based on registry fn :/

// for now expect it to be connected to the same cluster?

export class Observable {
  constructor(options: ObservableOptions, id: string = '', uuid?: string) {
    console.log('make observable for you')
    // on create need to connect
    // what about move subscription event?
    // just go trough all the subs managers
    // whats wrong here is that we dont know if you connect to multiple registries
    if (!uuid) {
      uuid = generateSubscriptionId(options)
    }
    this.uuid = uuid
    this.selvaId = String('o' + ++observableIds)

    this.id = id

    observables.set(uuid + id, this)
    // add to observables map
    // but - twist - need to add to specific port / host of a registry (annoying!)
  }

  public id: string

  public uuid: string // hash of getoptions

  public clients: Set<SelvaClient> = new Set()

  public selvaId: string

  public hardDisconnect() {
    console.log('hdc on obs bitch')
  }

  public attachClient(client: SelvaClient) {
    this.clients.add(client)
  }

  public destroyIfIdle() {
    // do it
  }

  public destroy() {
    observables.delete(this.uuid + this.id)
    // emit
  }

  public removeClient(client: SelvaClient): boolean {
    const hasClient = this.clients.has(client)
    if (hasClient) {
      this.clients.delete(client)
    }
    if (this.clients.size === 0) {
      this.destroyIfIdle()
    }
    return hasClient
  }
}

// maybe have to remove the re-use... :/
export const createObservable = (
  options: ObservableOptions,
  id: string = ''
) => {
  const uuid = generateSubscriptionId(options)
  let observable = observables.get(uuid + id)
  if (!observable) {
    observable = new Observable(options, id, uuid)
  }
  return observable
}
