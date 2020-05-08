import { ServerType, connect, SelvaClient } from '@saulx/selva'
import { ServerOptions } from '../types'
import { EventEmitter } from 'events'
import startRedis from './startRedis'
import chalk from 'chalk'

export class SelvaServer extends EventEmitter {
  public type: ServerType
  public port: number
  public registry: SelvaClient

  constructor(type: ServerType) {
    super()
    this.setMaxListeners(10000)
    this.type = type
  }

  start(opts: ServerOptions) {
    console.info(
      `Start SelvaServer ${chalk.white(opts.name)} of type ${chalk.blue(
        this.type
      )} on port ${chalk.blue(opts.port)}`
    )

    startRedis(this, opts)

    if (opts.registry) {
      this.registry = connect(opts.registry)
      console.log('create registry client')
      // important to define that you want to get stuff from the registry! - do it in nested methods
      // in get and set you can also pass 'registry'
    }
    // after this check what type you are
    // check if opts.registry
    // handle monitoring to registry
  }

  destroy() {}
}

export const startServer = (
  type: ServerType,
  opts: ServerOptions
): SelvaServer => {
  const server = new SelvaServer(type)
  server.start(opts)
  return server
}
