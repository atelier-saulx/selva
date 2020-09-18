export type LogEntry = { level: LogLevel; msg: string; clientId: string }
export type LogLevel = 'info' | 'notice' | 'warning' | 'error' | 'off'
export type LogFn = (log: LogEntry, dbName: string) => void

export type ClientOpts = {
  loglevel?: LogLevel
  log?: LogFn
  serverType?: string
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

// maybe add registry here?
export type ServerSelector = {
  name?: string
  type?: ServerType
  host?: string
  strict?: boolean
  port?: number
  subscription?: string
}

export type ServerSelectOptions = { subscription?: string; strict?: true }

// complete server selector + registry client

// TODO: make non optional
export type ServerDescriptor = {
  name?: string // tmp to test things
  type?: ServerType // tmp to test things
  host: string
  port: number
  index?: number
  stats?: any
}

export type Servers = Record<string, Record<string, ServerDescriptor[]>>

export type ServersById = Record<string, ServerDescriptor>
