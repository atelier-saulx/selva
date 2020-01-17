type args = (string | number)[]

abstract class RedisMethods {
  abstract queue(
    command: string,
    args: args,
    resolve: (x: any) => void,
    reject: (x: Error) => void,
    subscriber?: boolean
  ): void

  async command(key: string, ...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue(key, args, resolve, reject)
    })
  }

  async dbsize(): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('dbsize', [], resolve, reject)
    })
  }

  async decr(key: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('decr', [key], resolve, reject)
    })
  }

  async incr(key: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('incr', [key], resolve, reject)
    })
  }

  async decrby(key: string, amount: number): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('decrby', [key, amount], resolve, reject)
    })
  }

  async incrby(key: string, amount: number): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('incrby', [key, amount], resolve, reject)
    })
  }

  async incrbyfloat(key: string, amount: number): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('incrbyfloat', [key, amount], resolve, reject)
    })
  }

  async set(key: string, value: any): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('set', [key, value], v => resolve(v === 'OK'), reject)
    })
  }

  async setnx(key: string, value: any): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('setnx', [key, value], v => resolve(v === 'OK'), reject)
    })
  }

  async get(key: string): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('get', [key], resolve, reject)
    })
  }

  async del(key: string): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('del', [key], v => resolve(v === 1), reject)
    })
  }

  async hdel(hash: string, ...fields: string[]): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('hdel', [hash, ...fields], v => resolve(v === 1), reject)
    })
  }

  async hexists(hash: string, field: string): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('hexists', [hash, field], v => resolve(v === 1), reject)
    })
  }

  async hget(hash: string, field: string): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('hget', [hash, field], resolve, reject)
    })
  }

  async hgetall(hash: string): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('hgetall', [hash], resolve, reject)
    })
  }

  async hincrby(hash: string, field: string, amount: number): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('hincrby', [hash, field, amount], resolve, reject)
    })
  }

  async hincrbyfloat(
    hash: string,
    field: string,
    amount: number
  ): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('hincrbyfloat', [hash, field, amount], resolve, reject)
    })
  }

  async hkeys(hash: string): Promise<string[]> {
    return new Promise((resolve, reject) => {
      this.queue('hkeys', [hash], resolve, reject)
    })
  }

  async hlen(hash: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('hlen', [hash], resolve, reject)
    })
  }

  async hmget(hash: string, ...field: string[]): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('hmget', [hash, ...field], resolve, reject)
    })
  }

  async hmset(hash: string, ...fieldValue: string[]): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue(
        'hmset',
        [hash, ...fieldValue],
        v => resolve(v === 'OK'),
        reject
      )
    })
  }

  async hset(hash: string, field: string, value: any): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('hset', [hash, field, value], v => resolve(v === 1), reject)
    })
  }

  async hsetnx(hash: string, field: string, value: any): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('hsetnx', [hash, field, value], v => resolve(v === 1), reject)
    })
  }

  async hstrlen(hash: string, field: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('hstrlen', [hash, field], resolve, reject)
    })
  }

  async hvals(hash: string): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('hvals', [hash], resolve, reject)
    })
  }

  async getbit(key: string, offset: number, offset2?: number): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('getbit', [key, offset, offset2], resolve, reject)
    })
  }

  async getrange(key: string, start: number, end: number): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('getrange', [key, start, end], resolve, reject)
    })
  }

  async getset(key: string, value: any): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('getset', [key, value], resolve, reject)
    })
  }

  async exists(key: string): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('exists', [key], v => resolve(v === 1), reject)
    })
  }

  async eval(...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('eval', args, resolve, reject)
    })
  }

  async evalsha(...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('eval', args, resolve, reject)
    })
  }

  async exec(): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('eval', [], resolve, reject)
    })
  }

  async expire(key: string, seconds: number): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('expire', [key], v => resolve(v === 1), reject)
    })
  }

  async expireat(key: string, timestamp: number): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('expireat', [key], v => resolve(v === 1), reject)
    })
  }

  async flushall(): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('flushall', [], v => resolve(v === 'OK'), reject)
    })
  }

  async flushdb(): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('flushdb', [], v => resolve(v === 'OK'), reject)
    })
  }

  async geoadd(...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('geoadd', args, resolve, reject)
    })
  }

  async geohash(...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('geohash', args, resolve, reject)
    })
  }

  async geopos(...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('geopos', args, resolve, reject)
    })
  }

  async geodist(...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('geodist', args, resolve, reject)
    })
  }

  async georadius(...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('georadius', args, resolve, reject)
    })
  }

  async georadiusbymember(...args: args): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('georadiusbymember', args, resolve, reject)
    })
  }

  async keys(pattern: string): Promise<string[]> {
    // double check if it uses scan internally node redis is supposed to be doing that
    return new Promise((resolve, reject) => {
      this.queue('keys', [pattern], resolve, reject)
    })
  }

  async multi(): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('multi', [], v => resolve(v === 'OK'), reject)
    })
  }

  async sadd(key: string, ...member: string[]): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('sadd', [key, ...member], v => resolve(v === 1), reject)
    })
  }

  async srem(key: string, ...member: string[]): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('srem', [key, ...member], v => resolve(v === 1), reject)
    })
  }

  async sismember(key: string, member: string): Promise<boolean> {
    return new Promise((resolve, reject) => {
      this.queue('sismember', [key, member], v => resolve(v === 1), reject)
    })
  }

  async smembers(key: string): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('smembers', [key], resolve, reject)
    })
  }

  async smove(
    source: string,
    destination: string,
    member: any
  ): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('smembers', [source, destination, member], resolve, reject)
    })
  }

  async scard(key: string): Promise<number> {
    return new Promise((resolve, reject) => {
      this.queue('scard', [key], resolve, reject)
    })
  }

  async sdiff(...key: string[]): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('sdiff', key, resolve, reject)
    })
  }

  async sunion(...key: string[]): Promise<any[]> {
    return new Promise((resolve, reject) => {
      this.queue('sunion', key, resolve, reject)
    })
  }

  async loadScript(script: string): Promise<string> {
    return new Promise((resolve, reject) => {
      this.queue('script', ['load', script], resolve, reject)
    })
  }

  async evalSha(
    sha: string,
    numKeys: number,
    ...keysAndArgs: string[]
  ): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('evalsha', [sha, numKeys, ...keysAndArgs], resolve, reject)
    })
  }

  async ftInfo(index: string): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('FT.INFO', [index], resolve, reject)
    })
  }

  // async ftIndex(index: string): Promise<any> {
  //   return new Promise((resolve, reject) => {
  //     this.queue('FT.INDEX', [index], resolve, reject)
  //   })
  // }
}

export default RedisMethods
