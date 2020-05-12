import { ServerDescriptor } from '../types'

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

export type Servers = Record<string, Record<string, ServerDescriptor[]>>

export type ServersById = Record<string, ServerDescriptor>
