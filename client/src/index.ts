import { EventEmitter } from 'events'
import { ConnectOptions, ServerDescriptor, ClientOpts, LogLevel, ServerType } from './types'
import digest from './digest'
import Redis from './redis'
import {GetSchemaResult, SchemaOptions, Id} from './schema'
import { updateSchema } from './schema/updateSchema'
import {GetOptions, GetResult, get} from './get'
import {SetOptions, set} from './set'
import {IdOptions} from 'lua/src/id'
import id from './id'
import {DeleteOptions, deleteItem} from './delete'

export * as constants from './constants'

export class SelvaClient extends EventEmitter {
  public clientId: string
  public redis: Redis

  constructor(opts: ConnectOptions, clientOpts?: ClientOpts) {
    super()
    this.setMaxListeners(10000)
    if (!clientOpts) {
      clientOpts = {}
    }
    this.redis = new Redis(this, opts, clientOpts)
  }

  id(props: IdOptions): Promise<string> {
    return id(this, props)
  }

  get(getOpts: GetOptions): Promise<GetResult> {
    return get(this, getOpts)
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

export { ConnectOptions, ServerType, ServerDescriptor, GetOptions }
