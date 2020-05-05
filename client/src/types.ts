export type LogEntry = { level: LogLevel; msg: string }
export type LogLevel = 'info' | 'notice' | 'warning' | 'error' | 'off'
export type LogFn = (log: LogEntry) => void

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
