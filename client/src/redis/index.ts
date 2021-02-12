import { ServerSelector } from '../types'
import { RedisCommand, Callback } from './types'
import RedisMethods from './methods'
import { SelvaClient } from '..'
import {
  addRemoteListener,
  removeRemoteListener,
} from '../updateRemoteListeners'
import addRemoteCommand from '../addRemoteCommand'

class RedisSelvaClient extends RedisMethods {
  selvaClient: SelvaClient

  constructor(selvaClient: SelvaClient) {
    super()
    this.selvaClient = selvaClient
  }

  on(selector: ServerSelector, event: string, callback: Callback): void
  on(event: string, callback: Callback): void
  on(selector: any, event: any, callback?: any): void {
    addRemoteListener(this.selvaClient, selector, event, callback)
  }

  removeListener(
    selector: ServerSelector,
    event: string,
    callback?: Callback
  ): void
  removeListener(event: string, callback: Callback): void
  removeListener(selector: any, event: any, callback?: any): void {
    removeRemoteListener(this.selvaClient, selector, event, callback)
  }

  addCommandToQueue(
    command: RedisCommand,
    selector: ServerSelector = { name: 'default' }
  ) {
    addRemoteCommand(this.selvaClient, command, selector)
  }
}

export default RedisSelvaClient
