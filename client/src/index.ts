// TODO: start remove dirty globals
declare global {
  module NodeJS {
    interface Global {
      SELVAS: Record<string, SelvaClient>
    }
  }
}

global.SELVAS = {}
// TODO: end remove dirty globals


import { EventEmitter } from 'events'
import { ConnectOptions, ClientOpts, LogLevel, ServerType } from './types'
import digest from './digest'
import Redis from './redis'
import {GetSchemaResult, SchemaOptions} from './schema'
import { updateSchema } from './schema/updateSchema'

export * as constants from './constants'

export class SelvaClient extends EventEmitter {
  public id: string
  public redis: Redis

  constructor(opts: ConnectOptions, clientOpts?: ClientOpts) {
    super()
    this.setMaxListeners(10000)
    if (!clientOpts) {
      clientOpts = {}
    }
    this.redis = new Redis(this, opts, clientOpts)
  }

  digest(payload: string) {
    return digest(payload)
  }

  getSchema(name?: string): Promise<GetSchemaResult> {
    return this.redis.getSchema({name: name || 'default'})
  }

  updateSchema(opts: SchemaOptions, name?: string): Promise<void> {
    return updateSchema(this, opts, name)
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

export { ConnectOptions, ServerType }
