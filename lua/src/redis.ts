export function hexists(key: string, field: string): boolean {
  const result = redis.call('hexists', field)
  return result === 1
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
  value: string,
  ...rest: string[]
): number {
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

export function sadd(key: string, ...members: string[]): boolean {
  const result = redis.call('sadd', ...members)
  return result === 1
}

export function smembers(key: string): string[] {
  return redis.call('smembers', key)
}

export function scard(key: string): number {
  return redis.call('scard', key)
}

export function srem(
  key: string,
  member: string,
  ...members: string[]
): boolean {
  return redis.call('srem', member, ...members)
}

export function exists(...keys: string[]): number {
  return redis.call('exists', ...keys)
}

export function del(key: string, ...keys: string[]): number {
  return redis.call('del', key, ...keys)
}

export function ping(): 'PONG' {
  return redis.call('ping')
}
