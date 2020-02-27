import * as redis from './redis'

export type LogLevel = 'info' | 'notice' | 'warning' | 'error'

let clientId: string | null = null
let enabledLogLevel: LogLevel = 'warning'

export function configureLogger(id: string | null = null, loglevel?: LogLevel) {
  clientId = id
  if (loglevel) {
    enabledLogLevel = loglevel
  }
}

function logLevelToNumber(loglevel: LogLevel): number {
  if (loglevel === 'info') {
    return 0
  } else if (loglevel === 'notice') {
    return 1
  } else if (loglevel === 'warning') {
    return 2
  } else if (loglevel === 'error') {
    return 3
  }

  // disable logging by default
  return 4
}

export function log(loglevel: LogLevel, ...args: any[]): void {
  if (!clientId) {
    return
  }

  const enabled = logLevelToNumber(enabledLogLevel)
  const used = logLevelToNumber(loglevel)

  if (used >= enabled) {
    let log: string = ''
    for (let i = 0; i < args.length; i++) {
      const msg = args[i]
      let asStr: string
      if (type(msg) === 'table') {
        asStr = cjson.encode(msg)
      } else if (type(msg) !== 'string') {
        asStr = tostring(msg)
      } else {
        asStr = msg
      }
      log += asStr + ' '
    }

    redis.log(clientId, loglevel, log)
  }
}

export function info(...msg: any[]): void {
  log('info', ...msg)
}

export function notice(...msg: any[]): void {
  log('notice', ...msg)
}

export function warn(...msg: any[]): void {
  log('warning', ...msg)
}

export function error(...msg: any[]): void {
  log('error', ...msg)
}
