import { EventEmitter } from 'events'
import {
  ConnectOptions,
  ServerDescriptor,
  ServerType,
  ServerSelector,
  ServerSelectOptions,
  LogFn
} from './types'
import digest from './digest'
import Redis from './redis'
import { GetSchemaResult, SchemaOptions, Id, Schema, FieldSchema } from './schema'
import { FieldSchemaObject } from './schema/types'
import { updateSchema } from './schema/updateSchema'
import { getSchema } from './schema/getSchema'
// import initializeSchema from './schema/initializeSchema'
import { GetOptions, GetResult, get } from './get'
import { SetOptions, set } from './set'
import { IdOptions } from 'lua/src/id'
import id from './id'
import { DeleteOptions, deleteItem } from './delete'
import { deleteType, deleteField, castField } from './adminOperations'
import { RedisCommand } from './redis/types'
import conformToSchema from './conformToSchema'


import { waitUntilEvent } from './util'

import hardDisconnect from './hardDisconnect'

import { connections, Connection, createConnection } from './connection'

import connectRegistry from './connectRegistry'


import destroy from './destroy'

import { v4 as uuidv4 } from 'uuid'

// import { observe, observeSchema} from './observe'

import getServer from './getServer'

// import Observable from './observe/observable'

export * as constants from './constants'

let clientId = 0

export class SelvaClient extends EventEmitter {
  public redis: Redis

  public selvaId: string

  public uuid: string

  // add these on the registry scince that is the thing that gets reused
  // public schemaObservables: Record<string, Observable<Schema>> = {}
  public schemas: Record<string, Schema> = {}

  public server: ServerDescriptor

  public addServerUpdateListeners: (() => void)[] = []

  public servers: {
    ids: Set<string>
    subsManagers: ServerDescriptor[]
    // replicas by name
    replicas: { [key: string]: ServerDescriptor[] }
    // origins by name
    origins: { [key: string]: ServerDescriptor }
  } = {
      ids: new Set(),
      origins: {},
      subsManagers: [],
      replicas: {}
    }

  public registryConnection?: Connection

  public logFn: LogFn

  public loglevel: string

  public async hardDisconnect(connection: Connection) {
    return hardDisconnect(this, connection)
  }

  public admin: {
    deleteType(name: string, dbName?: string): Promise<void>,
    deleteField(type: string, name: string, dbName?: string): Promise<void>,
    castField(type: string, name: string, newType: FieldSchema, dbName?: string): Promise<void>
  } = {
      deleteType: (name: string, dbName: string = 'default') => {
        return deleteType(this, name, { name: dbName })
      },
      deleteField: (type: string, name: string, dbName: string = 'default') => {
        return deleteField(this, type, name, { name: dbName })
      },
      castField: (type: string, name: string, newType: FieldSchema, dbName: string = 'default') => {
        return castField(this, type, name, newType, { name: dbName })
      }
    }

  constructor(opts: ConnectOptions) {
    super()
    this.setMaxListeners(1e5)
    // tmp for logs
    this.uuid = uuidv4()
    this.selvaId = ++clientId + ''
    this.redis = new Redis(this)
    connectRegistry(this, opts)


  }

  connect(opts: ConnectOptions) {
    console.log('maybe bit different name? connect :/')
    connectRegistry(this, opts)
    // diffrent name
  }

  logLevel() {
    // for logs its connection uuid + client id
    // can enable / disable logleves
  }

  async initializeSchema(opts: any) {
    // return initializeSchema(this, opts)
  }

  async id(props: IdOptions): Promise<string> {
    // make this with $
    await this.initializeSchema({ $db: props.db || 'default' })
    return id(this, props)
  }

  async get(getOpts: GetOptions): Promise<GetResult> {
    return get(this, getOpts)
  }


  async set(setOpts: SetOptions): Promise<Id | undefined> {
    await this.initializeSchema(setOpts)
    return set(this, setOpts)
  }

  async delete(deleteOpts: DeleteOptions): Promise<boolean> {
    await this.initializeSchema(deleteOpts)
    return deleteItem(this, deleteOpts)
  }

  digest(payload: string) {
    return digest(payload)
  }

  getSchema(name: string = 'default'): Promise<GetSchemaResult> {
    return getSchema(this, { name: name })
  }

  async updateSchema(
    opts: SchemaOptions,
    name: string = 'default'
  ): Promise<void> {
    await this.initializeSchema({ $db: name })
    return updateSchema(this, opts, { name })
  }

  async waitUntilEvent(event: string): Promise<void> {
    return waitUntilEvent(this, event)
  }

  // subscribeSchema(name: string = 'default'): Observable<Schema> {
  //   //  call this observeSchema....
  //   console.warn('subscribeSchema changed to observeSchema will be removed in future versions')
  //   return observeSchema(this, name)
  // }

  // observeSchema(name: string = 'default'): Observable<Schema> {
  //   return observeSchema(this, name)
  // }

  // observe(props: GetOptions) {
  //   return observe(this, props)
  // }

  async conformToSchema(props: SetOptions, dbName: string = 'default') {
    await this.initializeSchema({ $db: dbName })
    return conformToSchema(this, props, dbName)
  }

  getServer(opts: ServerSelector, selectOptions?: ServerSelectOptions): Promise<ServerDescriptor> {
    return new Promise(r => {
      getServer(this, r, opts, selectOptions)
    })
  }

  async destroy() {
    return destroy(this)
  }
}

export function connect(
  opts: ConnectOptions
): SelvaClient {
  const client = new SelvaClient(opts)
  return client
}

const moduleId = ~~(Math.random() * 100000)

export {
  connections,
  createConnection,
  ConnectOptions,
  ServerType,
  ServerDescriptor,
  GetOptions,
  FieldSchemaObject,
  RedisCommand,
  Connection,
  moduleId
}
