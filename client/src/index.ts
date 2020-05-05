import { EventEmitter } from 'events'
import { v4 as uuid } from 'uuid'
import { ConnectOptions, ClientOpts, LogLevel, ServerType } from './types'
import digest from './digest'

export class SelvaClient extends EventEmitter {
  public id: string
  public loglevel: LogLevel = 'off'
  // public redis: Redis

  constructor(opts: ConnectOptions, clientOpts?: ClientOpts) {
    super()
    this.setMaxListeners(10000)
    this.id = uuid()

    if (clientOpts && clientOpts.loglevel) {
      this.loglevel = clientOpts.loglevel
    } else {
      this.loglevel = 'off'
      if (!clientOpts) {
        clientOpts = {}
      }
      clientOpts.loglevel = 'off'
    }
    // this.redis = new Redis(opts, this, clientOpts)
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
