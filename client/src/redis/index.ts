import { EventEmitter } from 'events'
import { SelvaClient } from '../'
import { ClientOpts, ConnectOptions } from '../types'
import { RedisCommand, Client, Type } from './types'
import RedisMethods from './methods'

class Redis extends RedisMethods {
  public selvaClient: SelvaClient

  public queue: RedisCommand[]

  constructor(
    selvaClient: SelvaClient,
    connectOptions: ConnectOptions,
    opts: ClientOpts
  ) {
    super()
    this.selvaClient = selvaClient
    // connect options is onmly
  }

  async addCommandToQueue(
    command: string,
    args: (string | number)[],
    resolve: (x: any) => void = () => {},
    reject: (x: Error) => void = () => {},
    opts: Type = { name: 'default' }
  ) {}
}
