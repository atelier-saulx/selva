import { ServerType, connect, SelvaClient } from '@saulx/selva'
import { ServerOptions } from '../types'
import { EventEmitter } from 'events'
import startRedis from './startRedis'
import chalk from 'chalk'
import ProcessManager from './processManager'
import attachStatusListeners from './attachStatusListeners'
import {
  startSubscriptionManager,
  stopSubscriptionManager,
  SubscriptionManagerState
} from './subscriptionManager'

export class SelvaServer extends EventEmitter {
  public type: ServerType
  public port: number
  public host: string
  public registry: SelvaClient
  public pm: ProcessManager
  public subscriptionManager: SubscriptionManagerState

  constructor(type: ServerType) {
    super()
    this.setMaxListeners(10000)
    this.type = type
  }

  async start(opts: ServerOptions) {
    console.info(
      `Start SelvaServer ${chalk.white(opts.name)} of type ${chalk.blue(
        this.type
      )} on port ${chalk.blue(String(opts.port))}`
    )

    this.port = opts.port
    this.host = opts.host

    if (
      !(
        typeof opts.registry === 'object' &&
        !(opts.registry instanceof Promise) &&
        opts.host === opts.registry.host &&
        opts.port === opts.registry.port
      )
    ) {
      await startRedis(this, opts)
    }

    if (opts.registry) {
      console.log('create registry client on the server')
      this.registry = connect(opts.registry)
      // important to define that you want to get stuff from the registry! - do it in nested methods
      // in get and set you can also pass 'registry'
    } else if (this.type === 'registry') {
      console.log('im the registry - register myself')
      this.registry = connect({ port: opts.port })
    }

    attachStatusListeners(this, opts)

    if (this.type === 'subscriptionManager') {
      this.subscriptionManager = await startSubscriptionManager(opts)
    }
  }

  async destroy() {
    if (this.pm) {
      this.pm.destroy()
      this.pm = undefined
    }
    if (this.type === 'subscriptionManager') {
      await stopSubscriptionManager(this.subscriptionManager)
    }

    this.emit('close')
  }
}

export const startServer = async (
  type: ServerType,
  opts: ServerOptions
): Promise<SelvaServer> => {
  const server = new SelvaServer(type)
  await server.start(opts)
  return server
}
