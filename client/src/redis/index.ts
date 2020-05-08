import { EventEmitter } from 'events'
import { SelvaClient } from '../'
import { ClientOpts, ConnectOptions } from '../types'
import { RedisCommand, Client, Type } from './types'
import RedisMethods from './methods'
import { v4 as uuidv4 } from 'uuid'

// now connect to registry make make

class Redis extends RedisMethods {
  public selvaClient: SelvaClient

  public queue: RedisCommand[]

  public id: string

  constructor(
    selvaClient: SelvaClient,
    connectOptions: ConnectOptions,
    opts: ClientOpts
  ) {
    super()
    this.id = uuidv4()
    this.selvaClient = selvaClient
  }

  addCommandToQueue(
    command: string,
    args: (string | number)[],
    resolve: (x: any) => void = () => {},
    reject: (x: Error) => void = () => {},
    opts: Type = { name: 'default' }
  ) {
    console.log('lullz', command)
  }

  drainQueue() {
    // drain it good
  }
}

export default Redis
