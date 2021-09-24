import { EventEmitter } from 'events'
import {
  ConnectOptions,
  ServerDescriptor,
  ServerType,
  ServerSelector,
  ServerSelectOptions,
} from './types'
import digest from './digest'
import Redis from './redis'
import {
  GetSchemaResult,
  SchemaOptions,
  Id,
  Schema,
  FieldSchema,
} from './schema'
import { FieldSchemaObject } from './schema/types'
import { updateSchema } from './schema/updateSchema'
import { getSchema } from './schema/getSchema'
import conformToSchema from './schema/conformToSchema'
import initializeSchema from './schema/initializeSchema'
import { TimeseriesClient } from './timeseries'
import { GetOptions, ObserveEventOptions, GetResult, get } from './get'
import { SetOptions, set, setWithMeta } from './set'
import { IdOptions } from 'lua/src/id'
import id from './id'
import { DeleteOptions, deleteItem } from './delete'
import { deleteType, deleteField, castField } from './adminOperations'
import { RedisCommand } from './redis/types'

import { waitUntilEvent } from './util'

import hardDisconnect from './hardDisconnect'

import { connections, Connection, createConnection } from './connection'

import { Observable, createObservable } from './observable'

import connectRegistry from './connectRegistry'

import destroy from './destroy'

import { v4 as uuidv4 } from 'uuid'

import getServer from './getServer'
import { ObservableOptions, ObsSettings } from './observable/types'
import { SetMetaResponse } from './set/types'
import TimeseriesCache from './timeseries/timeseriesCache'

export * as constants from './constants'

let clientId = 0

export class SelvaClient extends EventEmitter {
  public redis: Redis
  public pg: TimeseriesClient

  public selvaId: string

  public uuid: string

  public observables: Map<string, Observable>

  // add these on the registry scince that is the thing that gets reused
  public schemaObservables: Map<string, Observable> = new Map()

  public schemas: Record<string, Schema> = {}

  public server: ServerDescriptor

  public timeseriesCache: TimeseriesCache

  public addServerUpdateListeners: (() => void)[] = []

  public servers: {
    ids: Set<string>
    subsManagers: ServerDescriptor[]
    // replicas by name
    replicas: { [key: string]: ServerDescriptor[] }
    // origins by name
    origins: { [key: string]: ServerDescriptor }
    subRegisters: { [key: string]: ServerDescriptor }
    timeseriesQueues: { [key: string]: ServerDescriptor }
    timeseries: { [key: string]: ServerDescriptor }
    tsRegisters: { [key: string]: ServerDescriptor }
  } = {
    ids: new Set(),
    origins: {},
    subsManagers: [],
    replicas: {},
    subRegisters: {},
    timeseries: {},
    tsRegisters: {},
    timeseriesQueues: {},
  }

  public registryConnection?: Connection

  public loglevel: string
  public isDestroyed: boolean

  public async hardDisconnect(connection: Connection) {
    return hardDisconnect(this, connection)
  }

  public admin: {
    deleteType(name: string, dbName?: string): Promise<void>
    deleteField(type: string, name: string, dbName?: string): Promise<void>
    castField(
      type: string,
      name: string,
      newType: FieldSchema,
      dbName?: string
    ): Promise<void>
  } = {
    deleteType: (name: string, dbName: string = 'default') => {
      return deleteType(this, name, { name: dbName })
    },
    deleteField: (type: string, name: string, dbName: string = 'default') => {
      return deleteField(this, type, name, { name: dbName })
    },
    castField: (
      type: string,
      name: string,
      newType: FieldSchema,
      dbName: string = 'default'
    ) => {
      return castField(this, type, name, newType, { name: dbName })
    },
  }

  constructor(opts: ConnectOptions) {
    super()
    this.setMaxListeners(1e5)
    // tmp for logs
    this.uuid = uuidv4()
    this.selvaId = ++clientId + ''
    this.redis = new Redis(this)
    connectRegistry(this, opts)
    this.pg = new TimeseriesClient(this)
  }

  connect(opts: ConnectOptions) {
    console.log('maybe bit different name? connect :/')
    connectRegistry(this, opts)
    // diffrent name
  }

  logLevel(loglevel: string) {
    this.loglevel = loglevel
    // for logs its connection uuid + client id
    // can enable / disable logleves
  }

  async initializeSchema(opts: any, waitForSchema: boolean = true) {
    return initializeSchema(this, opts, waitForSchema)
  }

  async id(props: IdOptions): Promise<string> {
    // make this with $
    await this.initializeSchema({ $db: props.db || 'default' })
    return id(this, props)
  }

  async get(getOpts: GetOptions): Promise<GetResult> {
    if (!getOpts) {
      throw new Error(`Get query expected, got ${getOpts}`)
    }

    return get(this, getOpts)
  }

  async set(setOpts: SetOptions): Promise<Id | undefined> {
    await this.initializeSchema(setOpts)
    return set(this, setOpts)
  }

  async setWithMeta(setOpts: SetOptions): Promise<SetMetaResponse | undefined> {
    await this.initializeSchema(setOpts)
    return setWithMeta(this, setOpts)
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
    await this.initializeSchema({ $db: name }, false)
    return updateSchema(this, opts, { name, type: 'origin' })
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

  public subscribeSchema(name: string = 'default'): Observable {
    const props: ObservableOptions = {
      type: 'schema',
      db: name,
    }

    if (!this.schemaObservables.get(name)) {
      const obs = createObservable(props, this)
      this.schemaObservables.set(name, obs)
      obs.subscribe((d) => {
        // console.log('incoming schema', d)
        this.schemas[name] = d
      })
      return obs
    } else {
      return this.schemaObservables.get(name)
    }
  }

  public observe(
    props: ObservableOptions | GetOptions,
    opts?: ObsSettings
  ): Observable {
    if (!props) {
      throw new Error(`Get query expected, got ${props}`)
    }

    if (props.type === 'get' || props.type === 'schema') {
      return createObservable(<ObservableOptions>props, this)
    }
    if (opts) {
      return createObservable(
        {
          type: 'get',
          options: props,
          ...opts,
        },
        this
      )
    } else {
      return createObservable(
        {
          type: 'get',
          options: props,
        },
        this
      )
    }
  }

  public observeEvent(
    event: 'created' | 'deleted' | 'updated',
    props: ObserveEventOptions
  ): Observable {
    const newProps: GetOptions = Object.assign({}, props, {
      $trigger: { $event: event },
    })
    if (props.$filter) {
      newProps.$trigger.$filter = props.$filter
      delete newProps.$filter
    }

    return createObservable(
      {
        type: 'get',
        options: newProps,
        immutable: true,
      },
      this
    )
  }

  public async conformToSchema(props: SetOptions, dbName: string = 'default') {
    await this.initializeSchema({ $db: dbName })
    return conformToSchema(this, props, dbName)
  }

  public getServer(
    opts: ServerSelector,
    selectOptions?: ServerSelectOptions
  ): Promise<ServerDescriptor> {
    return new Promise((resolve) => {
      getServer(this, resolve, opts, selectOptions)
    })
  }

  async destroy() {
    return destroy(this)
  }
}

export function connect(
  opts: ConnectOptions,
  specialOpts?: { loglevel?: string }
): SelvaClient {
  const client = new SelvaClient(opts)
  if (specialOpts && specialOpts.loglevel) {
    client.logLevel(specialOpts.loglevel)
  }
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
  SetOptions,
  Schema,
  FieldSchemaObject,
  RedisCommand,
  Connection,
  Observable,
  moduleId,
}
