import { EventEmitter } from 'events'
import { ConnectOptions, ServerDescriptor, ClientOpts, LogLevel, ServerType, ServerSelector } from './types'
import digest from './digest'
import Redis from './redis'
import { GetSchemaResult, SchemaOptions, Id, Schema } from './schema'
import { FieldSchemaObject } from './schema/types'
import { updateSchema } from './schema/updateSchema'
import {GetOptions, GetResult, get} from './get'
import {SetOptions, set} from './set'
import {IdOptions} from 'lua/src/id'
import id from './id'
import {DeleteOptions, deleteItem} from './delete'
import { RedisCommand } from './redis/types'
import { v4 as uuidv4 } from 'uuid'
import { observe , observeSchema} from './observe'
import conformToSchema from './conformToSchema'
import getServerDescriptor from './redis/getServerDescriptor'
import Observable from './observe/observable'

export * as constants from './constants'

export class SelvaClient extends EventEmitter {
  public redis: Redis
  public uuid: string
  public loglevel: string
  public schemaObservables: Record<string, Observable<Schema>> = {}
  public schemas: Record<string, Schema> = {}

  constructor(opts: ConnectOptions, clientOpts?: ClientOpts) {
    super()
    // uuid is used for logs
    this.uuid = uuidv4()
    this.setMaxListeners(10000)
    if (!clientOpts) {
      clientOpts = {  }
    }

    this.loglevel = clientOpts.loglevel || 'warning'

    this.redis = new Redis(this, opts, clientOpts)
  }

  id(props: IdOptions): Promise<string> {
    return id(this, props)
  }

  get(getOpts: GetOptions): Promise<GetResult> {
    return get(this, getOpts)
  }

  observe(props: GetOptions) {
    return observe(this, props)
  }

  set(setOpts: SetOptions): Promise<Id | undefined> {
    return set(this, setOpts)
  }

  delete(deleteOpts: DeleteOptions): Promise<boolean> {
    return deleteItem(this, deleteOpts)
  }

  digest(payload: string) {
    return digest(payload)
  }

  getSchema(name: string = 'default'): Promise<GetSchemaResult> {
    return this.redis.getSchema({ name: name })
  }

  updateSchema(opts: SchemaOptions, name: string = 'default'): Promise<void> {
    return updateSchema(this, opts, { name })
  }

  subscribeSchema(name: string = 'default'): Observable<Schema> {
    return observeSchema(this, name)
  }
  
  conformToSchema(props: SetOptions, dbName: string = 'default') {
    return conformToSchema(this, props, dbName)
  }

  
  getServerDescriptor (opts: ServerSelector): Promise<ServerDescriptor> {
    return getServerDescriptor(this.redis,opts )
  }

  destroy() {
    console.log('destroy client - not implemented yet!')
  }
}

export function connect(
  opts: ConnectOptions,
  selvaOpts?: ClientOpts
): SelvaClient {
  const client = new SelvaClient(opts, selvaOpts)
  return client
}

export { ConnectOptions, ServerType, ServerDescriptor, GetOptions, FieldSchemaObject, RedisCommand }
