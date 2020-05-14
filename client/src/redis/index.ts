import { SelvaClient } from '../'
import { ClientOpts, ConnectOptions, ServerSelector } from '../types'
import { RedisCommand, Servers, ServersById, Callback } from './types'
import RedisMethods from './methods'
import { GetSchemaResult } from '../schema/types'
import { getClient, Client, addCommandToQueue } from './clients'
import getSchema from './getSchema'
import connectRegistry from './connectRegistry'
import getServerDescriptor from './getServerDescriptor'
import handleListener from './handleListener'

// add schema handling subscriptions / unsubscribe destorying making clients
class RedisSelvaClient extends RedisMethods {
  public selvaClient: SelvaClient
  public queue: { command: RedisCommand; selector: ServerSelector }[] = []
  public listenerQueue: {
    selector: ServerSelector
    event: string
    callback: Callback
  }[] = []

  public registry: Client

  public servers: Servers

  public serversById: ServersById

  constructor(
    selvaClient: SelvaClient,
    connectOptions: ConnectOptions,
    opts: ClientOpts
  ) {
    super()
    this.selvaClient = selvaClient
    connectRegistry(this, connectOptions)
  }

  async getSchema(selector: ServerSelector): Promise<GetSchemaResult> {
    return getSchema(this, selector)
  }

  on(selector: ServerSelector, event: string, callback: Callback): void
  on(event: string, callback: Callback): void
  on(selector: any, event: any, callback?: any): void {
    handleListener(this, 'on', selector, event, callback)
  }

  removeListener(
    selector: ServerSelector,
    event: string,
    callback: Callback
  ): void
  removeListener(event: string, callback: Callback): void
  removeListener(selector: any, event: any, callback?: any): void {
    handleListener(this, 'removeListener', selector, event, callback)
  }

  addCommandToQueue(
    command: RedisCommand,
    selector: ServerSelector = { name: 'default' }
  ) {
    if (!this.registry) {
      this.queue.push({ command, selector })
    } else {
      if (selector.type === 'registry' || selector.name === 'registry') {
        // this is nessecary scince you need to start somewhere
        addCommandToQueue(this.registry, command)
      } else {
        getServerDescriptor(this, selector).then(descriptor => {
          addCommandToQueue(
            getClient(
              this,
              descriptor.name,
              descriptor.type,
              descriptor.port,
              descriptor.host
            ),
            command
          )
        })
      }
    }
  }
}

export default RedisSelvaClient
