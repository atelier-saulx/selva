import { default as RedisClient, ConnectOptions } from './redis'
import id from './id'

export class SelvaClient {
  public redis: RedisClient

  constructor(opts: ConnectOptions | (() => Promise<ConnectOptions>)) {
    this.redis = new RedisClient(opts)
  }

  // need client.destroy (at least for tests)

  id = id
}

export function connect(
  opts: ConnectOptions | (() => Promise<ConnectOptions>)
): SelvaClient {
  return new SelvaClient(opts)
}
