import { ServerType } from '@saulx/selva'
import { ServerOptions } from '../types'
import { EventEmitter } from 'events'
import start from './start'

export class SelvaServer extends EventEmitter {
  public type: ServerType
  public port: number

  constructor(type: ServerType) {
    super()
    this.setMaxListeners(10000)
    this.type = type
  }

  start(opts: ServerOptions) {
    start(this, opts)
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
