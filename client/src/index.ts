import { EventEmitter } from 'events'
import {
  ConnectOptions,
  ServerDescriptor,
  ClientOpts,
  ServerType,
  ServerSelector,
  LogFn,
  LogEntry
} from './types'
import digest from './digest'
import Redis from './redis'
import { GetSchemaResult, SchemaOptions, Id, Schema, FieldSchema } from './schema'
import { FieldSchemaObject } from './schema/types'
import { updateSchema } from './schema/updateSchema'
import { getSchema } from './schema/getSchema'
import { GetOptions, GetResult, get } from './get'
import { SetOptions, set } from './set'
import { IdOptions } from 'lua/src/id'
import id from './id'
import { DeleteOptions, deleteItem } from './delete'
import { deleteType, deleteField, castField } from './adminOperations'
import { RedisCommand } from './redis/types'
import { v4 as uuidv4 } from 'uuid'
import { observe, observeSchema } from './observe'
import conformToSchema from './conformToSchema'
import getServerDescriptor from './redis/getServerDescriptor'
import Observable from './observe/observable'

export * as constants from './constants'

export class SelvaClient extends EventEmitter {
  public redis: Redis
  public uuid: string
  public logFn: LogFn
  public loglevel: string
  public schemaObservables: Record<string, Observable<Schema>> = {}
  public schemas: Record<string, Schema> = {}
  public serverType: string
 
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

  constructor(opts: ConnectOptions, clientOpts?: ClientOpts) {
    super()
    // uuid is used for logs
    this.uuid = uuidv4()
    this.setMaxListeners(10000)
    if (!clientOpts) {
      clientOpts = {}
    }

    this.loglevel = clientOpts.loglevel || 'warning'
    this.logFn =
      clientOpts.log ||
      ((l: LogEntry, dbName: string) =>
        console.log(`LUA: [{${dbName}} ${l.level}] ${l.msg}`))

    this.on('log', ({ dbName, log }) => {
      this.logFn(log, dbName)
    })

    this.serverType = clientOpts.serverType

    this.redis = new Redis(this, opts)
  }

  private async initializeSchema(opts: any) {
    const dbName = (typeof opts === 'object' && opts.$db) || 'default'

    if (!this.schemas[dbName]) {
      await this.getSchema(dbName)
    }

    if (!this.schemaObservables[dbName]) {
      this.subscribeSchema()
    }
  }

  async id(props: IdOptions): Promise<string> {
    await this.initializeSchema({ $db: props.db || 'default' })
    return id(this, props)
  }

  async get(getOpts: GetOptions): Promise<GetResult> {
    return get(this, getOpts)
  }

  observe(props: GetOptions) {
    return observe(this, props)
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

  subscribeSchema(name: string = 'default'): Observable<Schema> {
    return observeSchema(this, name)
  }

  conformToSchema(props: SetOptions, dbName: string = 'default') {
    return conformToSchema(this, props, dbName)
  }

  getServerDescriptor(opts: ServerSelector): Promise<ServerDescriptor> {
    return getServerDescriptor(this.redis, opts)
  }

  destroy() {
    // console.log('destroy client - not implemented yet!')
  }
}

export function connect(
  opts: ConnectOptions,
  selvaOpts?: ClientOpts
): SelvaClient {
  const client = new SelvaClient(opts, selvaOpts)
  return client
}

export {
  ConnectOptions,
  ServerType,
  ServerDescriptor,
  GetOptions,
  FieldSchemaObject,
  RedisCommand
}
