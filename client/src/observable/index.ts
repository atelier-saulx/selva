// re-use some code here
// also diffing is handled here

import { GetOptions } from '../get'
import { SelvaClient } from '..'

import { createHash } from 'crypto'

function makeSubscriptionId(opts: GetOptions) {
  const hash = createHash('sha256')
  hash.update(JSON.stringify(opts))
  return hash.digest('hex')
}

// has state as well

const observables: Map<string, Observable> = new Map()

// is it an emitter?

// if an observer is disconntected from a connection - reconnect it

// next to selvaClients a connections get Observables as well (they are also its clients)

// Observables get a way to handlke hdc theneselves

// OBservables store things the same way as connections - with selvaClients in there

// logs are easier to handle just add the id in the command to the other id

// ignore schema for now scince everyting subscribes to it (annoying! we dont want this)

// observer
// add observer

// one observer from the connection

// counter for selvaClient

// if 0 delete and remove 'interest' from the connection - kill after a while

// SCHEMA SUBS JUST MAKE IT DONT CARE -- on destroy destroy it

// no more layers of observers is great

// on incoming update observer

// for now just get a subs manager keep it simple

// gets a destroy function as well ? (for ubnsubscribe / connection gets killed)

// what about hdc? want to keep them active? for a while is selva client still is connected ot them

// makes reuse a lot better

// so observables are just as connections seperate entities

var observableIds = 0

// maybe not only get options

// need a typeof check! tony help

export class Observable {
  constructor(getOptions: GetOptions, uuid?: string) {
    console.log('make observable for you')

    if (!uuid) {
      uuid = makeSubscriptionId(getOptions)
    }

    this.uuid = uuid

    this.selvaId = String('o' + ++observableIds)
  }

  public uuid: string // hash of getoptions

  public clients: Set<SelvaClient> = new Set()

  public selvaId: string

  public hardDisconnect() {
    console.log('hdc bitch')
  }

  public attachClient(client: SelvaClient) {
    this.clients.add(client)
  }

  public destroyIfIdle() {
    // do it
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

export const createConnection = (getOptions: GetOptions) => {
  const uuid = makeSubscriptionId(getOptions)
  let observable = observables.get(uuid)
  if (!observable) {
    observable = new Observable(observable, uuid)
  }
  return observable
}
