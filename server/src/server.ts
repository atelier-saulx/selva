import { ConnectOptions, ServerType } from '@saulx/selva'
import { ServerOptions } from './types'
import { EventEmitter } from 'events'

export class SelvaServer extends EventEmitter {
  public type: string
  public port: number
  constructor(type) {
    super()
    this.setMaxListeners(10000)
    this.type = type
  }

  start() {
    // replace this with a seperare process etc etc
  }
}

export const startServer = (
  type: ServerType,
  opts: ServerOptions
): SelvaServer => {
  const server = new SelvaServer(type)

  if (type === 'origin') {
  } else if (type === 'registry') {
    console.log('create registry go', type)
  } else if (type === 'replica') {
    console.log(type, 'not implemented yet')
  } else if (type === 'subscriptionManager') {
    console.log(type, 'not implemented yet')
  }

  return server
}
