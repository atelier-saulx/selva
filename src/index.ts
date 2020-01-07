import { default as RedisClient, ConnectOptions } from './redis'
import id from './id'
import set from './set'

export class SelvaClient {
  public redis: RedisClient

  constructor(opts: ConnectOptions | (() => Promise<ConnectOptions>)) {
    this.redis = new RedisClient(opts)
  }

  // need client.destroy (at least for tests)

  id = id
  set = set
}

export function connect(
  opts: ConnectOptions | (() => Promise<ConnectOptions>)
): SelvaClient {
  return new SelvaClient(opts)
}
