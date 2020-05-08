import { SelvaClient } from '../'
import { ClientOpts, ConnectOptions } from '../types'
import { RedisCommand, Type, Client } from './types'
import RedisMethods from './methods'
import { v4 as uuidv4 } from 'uuid'
import { getClient } from './clients'

// now connect to registry make make
// re attach to different clients if they stop working

class Redis extends RedisMethods {
  public selvaClient: SelvaClient

  public queue: RedisCommand[]

  public registry: Client

  public id: string

  constructor(
    selvaClient: SelvaClient,
    connectOptions: ConnectOptions,
    opts: ClientOpts
  ) {
    super()
    this.id = uuidv4()
    this.selvaClient = selvaClient

    // opts for logs

    if (
      typeof connectOptions !== 'function' &&
      !(connectOptions instanceof Promise)
    ) {
      console.log('start with non async connect')
      // need an emitter or attach to publisher
      this.registry = getClient(
        this,
        'registry',
        'registry',
        connectOptions.port,
        connectOptions.host
      )
    }
    // connect to registy here
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

  async drainQueue() {
    // here we handle which command has to go to which server
    // await update of registry
    // drain it good
  }
}

export default Redis
