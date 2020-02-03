import * as redis from './redis'

export type LogLevel = 'info' | 'notice' | 'warning' | 'error'

const enabledLogLevel: LogLevel = <LogLevel>redis.get('___selva_lua_loglevel')

function logLevelToNumber(loglevel: LogLevel): number {
  if (loglevel === 'notice') {
    return 1
  } else if (loglevel === 'warning') {
    return 2
  } else if (loglevel === 'error') {
    return 3
  }

  return 0
}

export function log(loglevel: LogLevel, ...args: any[]): void {
  const enabled = logLevelToNumber(enabledLogLevel)
  const used = logLevelToNumber(loglevel)

  if (used >= enabled) {
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
      redis.log(loglevel, asStr)
    }
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
