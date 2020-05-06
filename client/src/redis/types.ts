import { RedisClient } from 'redis'
import { ServerType } from '../types'

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

export type Type = {
  name: string
  id?: string // url:port for specifics e.g. a single replica, a single subscription manager
}

export type Client = {
  subscriber: RedisClient
  publisher: RedisClient
  queue: RedisCommand[]
  name: string // for logging
  id: string // url:port
  connected: boolean
  bufferInProgress: boolean
  busy: boolean // can be written from the registry
  heartbeatTimout: NodeJS.Timeout
  type: ServerType
}
