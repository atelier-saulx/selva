import { ClientType } from './redisWrapper'
import RedisClient from './'

type args = (string | number)[]

class RedisMethodsByType {
  constructor(redis: RedisClient) {
    this.redis = redis
  }

  public redis: RedisClient
  queue(
    command: string,
    args: (string | number)[],
    resolve: (x: any) => void,
    reject: (x: Error) => void,
    type: ClientType
  ): void {
    this.redis.queue(command, args, resolve, reject, type)
  }

  async command(type: ClientType, key: string, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue(key, args, resolve, reject, type)
    })
  }

  async dbsize(type: ClientType): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('dbsize', [], resolve, reject, type)
    })
  }

  async decr(type: ClientType, key: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('decr', [key], resolve, reject, type)
    })
  }

  async incr(type: ClientType, key: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('incr', [key], resolve, reject, type)
    })
  }

  async decrby(type: ClientType, key: string, amount: number): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('decrby', [key, amount], resolve, reject, type)
    })
  }

  async incrby(type: ClientType, key: string, amount: number): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('incrby', [key, amount], resolve, reject, type)
    })
  }

  async incrbyfloat(
    type: ClientType,
    key: string,
    amount: number
  ): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('incrbyfloat', [key, amount], resolve, reject, type)
    })
  }

  async set(type: ClientType, key: string, value: any): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('set', [key, value], v => resolve(v === 'OK'), reject, type)
    })
  }

  async setnx(type: ClientType, key: string, value: any): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('setnx', [key, value], v => resolve(v === 'OK'), reject, type)
    })
  }

  async get(type: ClientType, key: string): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('get', [key], resolve, reject, type)
    })
  }

  async del(type: ClientType, key: string): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('del', [key], v => resolve(v === 1), reject, type)
    })
  }

  async hdel(
    type: ClientType,
    hash: string,
    ...fields: string[]
  ): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('hdel', [hash, ...fields], v => resolve(v === 1), reject, type)
    })
  }

  async hexists(
    type: ClientType,
    hash: string,
    field: string
  ): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('hexists', [hash, field], v => resolve(v === 1), reject, type)
    })
  }

  async hget(type: ClientType, hash: string, field: string): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('hget', [hash, field], resolve, reject, type)
    })
  }

  async hgetall(
    type: ClientType,
    hash: string
  ): Promise<Record<string, string>> {
    return new Promise((resolve, reject) => {
      this.queue('hgetall', [hash], resolve, reject, type)
    })
  }

  async hincrby(
    type: ClientType,
    hash: string,
    field: string,
    amount: number
  ): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('hincrby', [hash, field, amount], resolve, reject, type)
    })
  }

  async hincrbyfloat(
    type: ClientType,
    hash: string,
    field: string,
    amount: number
  ): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('hincrbyfloat', [hash, field, amount], resolve, reject, type)
    })
  }

  async hkeys(type: ClientType, hash: string): Promise<string[]> {
    return new Promise((resolve, reject) => {
      this.queue('hkeys', [hash], resolve, reject, type)
    })
  }

  async hlen(type: ClientType, hash: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('hlen', [hash], resolve, reject, type)
    })
  }

  async hmget(
    type: ClientType,
    hash: string,
    ...field: string[]
  ): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('hmget', [hash, ...field], resolve, reject, type)
    })
  }

  async hmset(
    type: ClientType,
    hash: string,
    ...fieldValue: string[]
  ): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue(
        'hmset',
        [hash, ...fieldValue],
        v => resolve(v === 'OK'),
        reject,
        type
      )
    })
  }

  async hset(
    type: ClientType,
    hash: string,
    field: string,
    value: any
  ): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue(
        'hset',
        [hash, field, value],
        v => resolve(v === 1),
        reject,
        type
      )
    })
  }

  async hsetnx(
    type: ClientType,
    hash: string,
    field: string,
    value: any
  ): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue(
        'hsetnx',
        [hash, field, value],
        v => resolve(v === 1),
        reject,
        type
      )
    })
  }

  async hstrlen(
    type: ClientType,
    hash: string,
    field: string
  ): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('hstrlen', [hash, field], resolve, reject, type)
    })
  }

  async hvals(type: ClientType, hash: string): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('hvals', [hash], resolve, reject, type)
    })
  }

  async getbit(
    type: ClientType,
    key: string,
    offset: number,
    offset2?: number
  ): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('getbit', [key, offset, offset2], resolve, reject, type)
    })
  }

  async getrange(
    type: ClientType,
    key: string,
    start: number,
    end: number
  ): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('getrange', [key, start, end], resolve, reject, type)
    })
  }

  async getset(type: ClientType, key: string, value: any): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('getset', [key, value], resolve, reject, type)
    })
  }

  async exists(type: ClientType, key: string): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('exists', [key], v => resolve(v === 1), reject, type)
    })
  }

  async eval(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('eval', args, resolve, reject, type)
    })
  }

  async evalsha(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('eval', args, resolve, reject, type)
    })
  }

  async exec(type: ClientType): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('eval', [], resolve, reject, type)
    })
  }

  async expire(
    type: ClientType,
    key: string,
    seconds: number
  ): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('expire', [key], v => resolve(v === 1), reject, type)
    })
  }

  async expireat(
    type: ClientType,
    key: string,
    timestamp: number
  ): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('expireat', [key], v => resolve(v === 1), reject, type)
    })
  }

  async flushall(type: ClientType): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('flushall', [], v => resolve(v === 'OK'), reject, type)
    })
  }

  async flushdb(type: ClientType): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('flushdb', [], v => resolve(v === 'OK'), reject, type)
    })
  }

  async geoadd(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('geoadd', args, resolve, reject, type)
    })
  }

  async geohash(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('geohash', args, resolve, reject, type)
    })
  }

  async geopos(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('geopos', args, resolve, reject, type)
    })
  }

  async geodist(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('geodist', args, resolve, reject, type)
    })
  }

  async georadius(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('georadius', args, resolve, reject, type)
    })
  }

  async georadiusbymember(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('georadiusbymember', args, resolve, reject, type)
    })
  }

  async keys(type: ClientType, pattern: string): Promise<string[]> {
    // double check if it uses scan internally node redis is supposed to be doing that
    return new Promise((resolve, reject) => {
      this.queue('keys', [pattern], resolve, reject, type)
    })
  }

  async multi(type: ClientType): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('multi', [], v => resolve(v === 'OK'), reject, type)
    })
  }

  async sadd(
    type: ClientType,
    key: string,
    ...member: string[]
  ): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('sadd', [key, ...member], v => resolve(v === 1), reject, type)
    })
  }

  async srem(
    type: ClientType,
    key: string,
    ...member: string[]
  ): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('srem', [key, ...member], v => resolve(v === 1), reject, type)
    })
  }

  async sismember(
    type: ClientType,
    key: string,
    member: string
  ): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue(
        'sismember',
        [key, member],
        v => resolve(v === 1),
        reject,
        type
      )
    })
  }

  async smembers(type: ClientType, key: string): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('smembers', [key], resolve, reject, type)
    })
  }

  async smove(
    type: ClientType,
    source: string,
    destination: string,
    member: any
  ): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue(
        'smembers',
        [source, destination, member],
        resolve,
        reject,
        type
      )
    })
  }

  async scard(type: ClientType, key: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('scard', [key], resolve, reject, type)
    })
  }

  async sdiff(type: ClientType, ...key: string[]): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('sdiff', key, resolve, reject, type)
    })
  }

  async sunion(type: ClientType, ...key: string[]): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('sunion', key, resolve, reject, type)
    })
  }

  async publish(
    type: ClientType,
    channel: string,
    value: string
  ): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('publish', [channel, value], resolve, reject, type)
    })
  }

  async zrange(
    type: ClientType,
    key: string,
    start: number,
    end: number
  ): Promise<string[]> {
    return new Promise((resolve, reject) => {
      this.queue('zrange', [key, start, end], resolve, reject, type)
    })
  }

  async loadScript(type: ClientType, script: string): Promise<string> {
    return new Promise((resolve, reject) => {
      this.queue('script', ['load', script], resolve, reject, type)
    })
  }

  async evalSha(
    type: ClientType,
    sha: string,
    numKeys: number,
    ...keysAndArgs: string[]
  ): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue(
        'evalsha',
        [sha, numKeys, ...keysAndArgs],
        resolve,
        reject,
        type
      )
    })
  }

  async ftInfo(type: ClientType, index: string): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('FT.INFO', [index], resolve, reject, type)
    })
  }

  async ftSearch(type: ClientType, index: string, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('FT.SEARCH', [index, ...args], resolve, reject, type)
    })
  }

  async ftAlter(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('FT.ALTER', args, resolve, reject, type)
    })
  }

  async ftCreate(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('FT.CREATE', args, resolve, reject, type)
    })
  }

  async ftIndex(type: ClientType, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('FT.INDEX', args, resolve, reject, type)
    })
  }

  async ftTagVals(
    type: ClientType,
    index: string,
    tagField: string
  ): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('FT.TAGVALS', [index, tagField], resolve, reject, type)
    })
  }
}

export default RedisMethodsByType
