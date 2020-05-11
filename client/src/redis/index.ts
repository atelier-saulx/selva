import { SelvaClient, ServerType } from '../'
import { ClientOpts, ConnectOptions } from '../types'
import { RedisCommand, Type } from './types'
import RedisMethods from './methods'
import { v4 as uuidv4 } from 'uuid'
import { getClient, Client, addCommandToQueue } from './clients'

// now connect to registry make make
// re attach to different clients if they stop working

type Callback = (payload: any) => void

class RedisSelvaClient extends RedisMethods {
  public selvaClient: SelvaClient

  public queue: RedisCommand[]
  public listenerQueue: { opts: Type; event: string; callback: Callback }[]

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

  on(opts: Type, event: string, callback: Callback): void
  on(event: string, callback: Callback): void

  on(opts: any, event: any, callback?: any): void {
    // same here if !registry
    if (typeof opts === 'string') {
      callback = event
      event = opts
      // if replica is available
      opts = { name: 'default', type: 'replica' }
    }

    if (opts.type === 'registry') {
      this.registry.publisher.on(event, callback)
    } else {
    }
  }

  addCommandToQueue(command: RedisCommand, type: Type = { name: 'default' }) {
    // needs to add to queue if registry does not exists
    if (type.type === 'registry') {
      addCommandToQueue(this.registry, command)
    } else {
    }
  }
}

export default RedisSelvaClient
