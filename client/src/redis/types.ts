import { EventEmitter } from 'events'
import { GetResult, GetOptions } from '../get/types'

type Resolvable = {
  resolve: (x: any) => void
  reject: (x: Error) => void
}

export type RedisCommand = Resolvable & {
  command: string
  type?: string
  args: (string | number)[]
  hash?: number
  nested?: Resolvable[]
}

export type Subscription = {
  channel: string
  emitter: EventEmitter
  getOpts: GetOptions
  count: number
}

export type UpdateEvent = {
  type: 'update'
  payload: GetResult
}

export type DeleteEvent = {
  type: 'delete'
}

export type HeartBeatEvent = {
  type: 'heartbeat'
}

export type Event = UpdateEvent | HeartBeatEvent | DeleteEvent
