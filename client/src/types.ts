export type LogEntry = { level: LogLevel; msg: string }
export type LogLevel = 'info' | 'notice' | 'warning' | 'error' | 'off'
export type LogFn = (log: LogEntry, dbName: string) => void

export type ClientOpts = {
  loglevel?: LogLevel
  log?: LogFn
}

export type ServerType =
  | 'origin'
  | 'subscriptionManager'
  | 'replica'
  | 'registry'

// port and host is allways the registry!
export type Connect = {
  port?: number
  host?: string
}

export type ConnectOptions =
  | Connect
  | (() => Promise<Connect>)
  | Promise<Connect>

export type ServerSelector = {
  name?: string
  type?: ServerType
  host?: string
  port?: number
}

// TODO: make non optional
export type ServerDescriptor = {
  name: string
  type: ServerType
  host: string
  port: number
  default?: boolean
}
