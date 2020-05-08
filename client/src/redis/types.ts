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
  name?: string
  type?: ServerType
  id?: string // url:port for specifics e.g. a single replica, a single subscription manager
}
