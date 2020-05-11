import { SelvaClient, ServerType, connect } from '../'
import { ClientOpts, ConnectOptions, ServerSelector } from '../types'
import { RedisCommand } from './types'
import RedisMethods from './methods'
import { v4 as uuidv4 } from 'uuid'
import { getClient, Client, addCommandToQueue } from './clients'

// now connect to registry make make
// re attach to different clients if they stop working

type Callback = (payload: any) => void

class RedisSelvaClient extends RedisMethods {
  public selvaClient: SelvaClient

  public queue: { command: RedisCommand; selector: ServerSelector }[] = []
  public listenerQueue: {
    selector: ServerSelector
    event: string
    callback: Callback
  }[] = []

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

  async getServerName(selector: ServerSelector): Promise<string> {
    if (selector.name) {
      return selector.name
    } else if (selector.type === 'registry') {
      return selector.name
    } else {
      // find it in the registry!
      return 'default'
    }
  }

  on(type: ServerSelector, event: string, callback: Callback): void
  on(event: string, callback: Callback): void
  on(selector: any, event: any, callback?: any): void {
    if (!this.registry) {
      this.listenerQueue.push({ selector, event, callback })
    } else {
      if (typeof selector === 'string') {
        callback = event
        event = selector
        // if replica is available
        selector = { name: 'default', type: 'replica' }
      }

      if (selector.type === 'registry') {
        this.registry.subscriber.on(event, callback)
      } else {
      }
    }
  }

  addCommandToQueue(
    command: RedisCommand,
    selector: ServerSelector = { name: 'default' }
  ) {
    if (!this.registry) {
      this.queue.push({ command, selector })
    } else {
      if (selector.type === 'registry') {
        addCommandToQueue(this.registry, command)
      } else {
      }
    }
  }
}

export default RedisSelvaClient
