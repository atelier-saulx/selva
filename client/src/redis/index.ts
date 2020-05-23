import { SelvaClient, ServerDescriptor } from '../'
import {
  ClientOpts,
  ConnectOptions,
  ServerSelector,
  LogFn,
  LogEntry
} from '../types'
import { RedisCommand, Servers, ServersById, Callback } from './types'
import RedisMethods from './methods'
import { GetSchemaResult } from '../schema/types'
import { getClient, Client, addCommandToQueue } from './clients'
import connectRegistry from './connectRegistry'
import getServerDescriptor from './getServerDescriptor'
import handleListener from './handleListener'
import Observable from '../observe/observable'
import { GetOptions, GetResult } from '../get/types'
import { createObservable, ObserverEmitter } from './observers'

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
  public logFn: LogFn

  public servers: Servers
  public serversById: ServersById
  public subsManagers: ServerDescriptor[]

  // dont rly need more then this
  public observables: Record<string, Observable<GetResult>> = {}
  public observerEmitters: Record<string, ObserverEmitter> = {}

  constructor(selvaClient: SelvaClient, connectOptions: ConnectOptions) {
    super()
    this.selvaClient = selvaClient
    connectRegistry(this, connectOptions)
  }

  observe(channel: string, props: GetOptions): Observable<GetResult> {
    return createObservable(this, channel, props)
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
          addCommandToQueue(getClient(this, descriptor), command)
        })
      }
    }
  }
}

export default RedisSelvaClient
