export type Callback = (...args: any[]) => void

export type Resolvable = {
  resolve?: (x: any) => void
  reject?: (x: Error) => void
}

export type RedisCommand = Resolvable & {
  command: string
  type?: string
  args: (string | number | Buffer)[]
  hash?: number
  id?: string // id can be used to filter actions on (e.g. selvaClient id)
}
