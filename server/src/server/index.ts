import { ServerType, connect, SelvaClient } from '@saulx/selva'
import { ServerOptions } from '../types'
import { EventEmitter } from 'events'
import startRedis from './startRedis'
import chalk from 'chalk'
import ProcessManager from './processManager'

export class SelvaServer extends EventEmitter {
  public type: ServerType
  public port: number
  public registry: SelvaClient
  public pm: ProcessManager

  constructor(type: ServerType) {
    super()
    this.setMaxListeners(10000)
    this.type = type
  }

  attachStatusListeners(opts: ServerOptions) {
    this.on('perf_metrics', v => {
      console.log('lullz perf_metrics NOW', v)
    })

    this.on('busy', () => {})

    this.on('subscription', () => {})

    const info = {
      name: opts.name,
      type: this.type,
      port: opts.port,
      host: opts.host
    }

    this.updateRegistry()
  }

  updateRegistry(info) {
    // this.registry
  }

  start(opts: ServerOptions) {
    console.info(
      `Start SelvaServer ${chalk.white(opts.name)} of type ${chalk.blue(
        this.type
      )} on port ${chalk.blue(String(opts.port))}`
    )

    startRedis(this, opts)

    if (opts.registry) {
      console.log('create registry client on the server')
      this.registry = connect(opts.registry)
      // important to define that you want to get stuff from the registry! - do it in nested methods
      // in get and set you can also pass 'registry'
    } else if (this.type === 'registry') {
      console.log('im the registry - register myself')
      this.registry = connect({ port: opts.port })
    }

    this.attachStatusListeners(opts)
    // after this check what type you are
    // check if opts.registry
    // handle monitoring to registry
  }

  destroy() {
    if (this.pm) {
      this.pm.destroy()
      this.pm = undefined
    }
  }
}

export const startServer = (
  type: ServerType,
  opts: ServerOptions
): SelvaServer => {
  const server = new SelvaServer(type)
  server.start(opts)
  return server
}
