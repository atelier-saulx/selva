import { LogLevel } from './logger'
import { emptyArray } from './util'

const DEFAULT_HIERARCHY = '___selva_hierarchy'

export function Error(errorMsg: string): Error {
  return redis.error_reply(errorMsg)
}

export function log(clientId: string, loglevel: LogLevel, msg: any): void {
  redis.call(
    'PUBLISH',
    `___selva_lua_logs:${clientId}`,
    cjson.encode({ level: loglevel, msg, clientId })
  )
}

export function id(externalIdStr?: string): string {
  if (externalIdStr) {
    return redis.call('selva.id', externalIdStr)
  }

  return redis.call('selva.id')
}

export function delnode(id: string) {
  return redis.call('SELVA.HIERARCHY.DEL', DEFAULT_HIERARCHY, id)
}

export function delnoderef(id: string, key: 'parents' | 'children') {
  return redis.call('SELVA.HIERARCHY.DELREF', DEFAULT_HIERARCHY, id, key)
}

export function children(id: string) {
  const o: string[] = redis.call('SELVA.HIERARCHY.CHILDREN', DEFAULT_HIERARCHY, id)
  return o.length > 0 ? o : emptyArray()
}

export function parents(id: string) {
  const o: string[] = redis.call('SELVA.HIERARCHY.PARENTS', DEFAULT_HIERARCHY, id)
  return o.length > 0 ? o : emptyArray()
}

export function ancestors(id: string) {
  const o: string[] = redis.call('SELVA.HIERARCHY.FIND', DEFAULT_HIERARCHY, 'bfs', 'ancestors', id)
  return o.length > 0 ? o : emptyArray()
}

export function descendants(id: string) {
  const o: string[] = redis.call('SELVA.HIERARCHY.FIND', DEFAULT_HIERARCHY, 'bfs', 'descendants', id)
  return o.length > 0 ? o : emptyArray()
}

export function hexists(key: string, field: string): boolean {
  const result = redis.call('hexists', key, field)
  return result === 1
}

export function hget(key: string, field: string): string {
  return redis.call('hget', key, field)
}

export function hgetall(key: string): string[] {
  return redis.call('hgetall', key)
}

export function hmget(key: string, field: string, ...fields: string[]): any[] {
  return redis.call('hmget', key, field, ...fields)
}

export function hkeys(key: string): string[] {
  return redis.call('hkeys', key)
}

export function hset(
  key: string,
  fieldKey: string,
  value: any,
  ...rest: string[]
): number {
  if (rest.length === 0) {
    return redis.call('hset', key, fieldKey, value)
  }

  return redis.call('hset', key, fieldKey, value, ...rest)
}

export function hsetnx(
  key: string,
  fieldKey: string,
  value: string,
  ...rest: string[]
): number {
  return redis.call('hsetnx', key, fieldKey, value, ...rest)
}

export function hdel(
  key: string,
  fieldKey: string,
  ...fieldKeys: string[]
): number {
  return redis.call('hdel', key, fieldKey, ...fieldKeys)
}

// NOTE: this only works in debug mode
export function debug(msg: string): void {
  redis.debug(msg)
}

export function hincrby(key: string, field: string, increment: number): number {
  return redis.call('hincrby', key, field, tostring(increment))
}

export function sadd(key: string, ...members: string[]): number {
  const result = redis.call('sadd', key, ...members)
  return result
}

export function sismember(key: string, value: string): boolean {
  const result = redis.call('sismember', key, value)
  return result === 1
}

export function smembers(key: string): string[] {
  return redis.call('smembers', key)
}

export function sunion(args: string[]): string[] {
  return redis.call('sunion', ...args)
}

export function scard(key: string): number {
  return redis.call('scard', key)
}

export function srem(key: string, ...members: string[]): boolean {
  return redis.call('srem', key, ...members)
}

export function zAddMultipleNew(key: string, ...rest: string[]): number {
  return redis.call('zadd', key, 'NX', ...rest)
}

export function zaddNew(key: string, score: number, value: string): number {
  return redis.call('zadd', key, 'NX', tostring(score), value)
}

export function zAddReplaceScore(
  key: string,
  score: number | string,
  value: string
): number {
  return redis.call('zadd', key, 'XX', tostring(score), value)
}

export function zrangeWithScores(
  key: string,
  start: number = 0,
  end: number = -1
): string[] {
  return redis.call('zrange', key, tostring(start), tostring(end), 'WITHSCORES')
}

export function zrange(
  key: string,
  start: number = 0,
  end: number = -1
): string[] {
  return redis.call('zrange', key, tostring(start), tostring(end))
}

export function zscore(key: string, member: string): number {
  return tonumber(redis.call('zscore', key, member))
}

export function exists(...keys: string[]): boolean {
  return redis.call('exists', ...keys) === 1
}

export function expire(key: string, timeInSeconds: number) {
  return redis.call('exists', key, tostring(timeInSeconds))
}

export function pexpire(key: string, timeInMs: number) {
  return redis.call('pexists', key, tostring(timeInMs))
}

export function set(key: string, val: string): boolean {
  return redis.call('set', key, val)
}

export function get(key: string): string {
  return redis.call('get', key)
}

export function del(key: string, ...keys: string[]): number {
  if (keys.length === 0) {
    return redis.call('del', key)
  }
  return redis.call('del', key, ...keys)
}

export function ping(): 'PONG' {
  return redis.call('ping')
}

// export function ftSearch(index: string, ...args: string[]): any[] {
//   return redis.call('ft.search', index, ...args)
// }
