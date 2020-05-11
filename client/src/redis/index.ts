import { SelvaClient, ServerType, connect } from '../'
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

  public queue: { command: RedisCommand; type: Type }[] = []
  public listenerQueue: { type: Type; event: string; callback: Callback }[] = []

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

    if (typeof connectOptions === 'function') {
    } else if (connectOptions instanceof Promise) {
    } else {
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

  on(type: Type, event: string, callback: Callback): void
  on(event: string, callback: Callback): void

  on(type: any, event: any, callback?: any): void {
    if (!this.registry) {
      this.listenerQueue.push({ type, event, callback })
    } else {
      if (typeof type === 'string') {
        callback = event
        event = type
        // if replica is available
        type = { name: 'default', type: 'replica' }
      }

      if (type.type === 'registry') {
        this.registry.subscriber.on(event, callback)
      } else {
      }
    }
  }

  addCommandToQueue(command: RedisCommand, type: Type = { name: 'default' }) {
    if (!this.registry) {
      this.queue.push({ command, type })
    } else {
      if (type.type === 'registry') {
        addCommandToQueue(this.registry, command)
      } else {
      }
    }
  }
}

export default RedisSelvaClient
