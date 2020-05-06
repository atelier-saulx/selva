import { EventEmitter } from 'events'
import { ConnectOptions, ClientOpts, LogLevel, ServerType } from './types'
import digest from './digest'
import Redis from './redis'

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
}

export function connect(
  opts: ConnectOptions,
  selvaOpts?: ClientOpts
): SelvaClient {
  const client = new SelvaClient(opts, selvaOpts)
  return client
}

export { ConnectOptions, ServerType }
