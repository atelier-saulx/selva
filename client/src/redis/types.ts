import { RedisClient } from 'redis'
import { ServerType } from '../types'
import Redis from './'

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
  name?: string
  type?: ServerType
  id?: string // url:port for specifics e.g. a single replica, a single subscription manager
}

export type Client = {
  subscriber: RedisClient
  publisher: RedisClient
  queue: RedisCommand[]
  queueInProgress: boolean

  name: string // for logging
  id: string // url:port
  connected: boolean
  busy: boolean // can be written from the registry
  type: ServerType

  heartbeatTimout?: NodeJS.Timeout

  scripts: {
    batchingEnabled: { [scriptSha: string]: boolean }
    sha: { [scriptName: string]: string }
  }

  clients: Set<Redis>
}
