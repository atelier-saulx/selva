import { GetResult, GetOptions } from '../get/types'
import Redis from './'
import { LogFn } from '..'

type Resolvable = {
  resolve?: (x: any) => void
  reject?: (x: Error) => void
}

export type RedisCommand = Resolvable & {
  command: string
  type?: string
  args: (string | number)[]
  hash?: number
}

// different heartbeat event will be gone
export type UpdateEvent = {
  type: 'update'
  payload: GetResult
  version?: string
}

export type CustomEvent = {
  type: string
  payload?: any
  version?: string
}

export type Event = UpdateEvent | CustomEvent

export type ClientObject = {
  connect: (type: string) => void
  disconnect: (type: string) => void
  message: (channel: string, message: { [key: string]: any }) => void
  log?: LogFn
  client: Redis
}
