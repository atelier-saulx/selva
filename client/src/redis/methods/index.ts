type args = (string | number)[]
import { Type } from '../types'

abstract class RedisMethods {
  abstract addCommandToQueue(
    command: string,
    args: (string | number)[],
    resolve: (x: any) => void,
    reject: (x: Error) => void,
    opts?: Type
  ): void

  async command(opts: Type, key: string, ...args: args): Promise<any>
  async command(key: string, ...args: args): Promise<any>
  async command(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        const [key, ...commandArgs] = args
        this.addCommandToQueue(<string>key, commandArgs, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue(opts, args, resolve, reject)
      })
    }
  }

  async multi(opts: Type, ...args: args): Promise<any>
  async multi(...args: args): Promise<any>
  async multi(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('multi', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('multi', [opts, ...args], resolve, reject)
      })
    }
  }

  async batch(opts: Type, ...args: args): Promise<any>
  async batch(...args: args): Promise<any>
  async batch(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('batch', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('batch', [opts, ...args], resolve, reject)
      })
    }
  }

  async select(opts: Type, ...args: args): Promise<any>
  async select(...args: args): Promise<any>
  async select(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('select', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('select', [opts, ...args], resolve, reject)
      })
    }
  }

  async monitor(opts: Type, ...args: args): Promise<any>
  async monitor(...args: args): Promise<any>
  async monitor(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('monitor', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('monitor', [opts, ...args], resolve, reject)
      })
    }
  }

  async quit(opts: Type, ...args: args): Promise<any>
  async quit(...args: args): Promise<any>
  async quit(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('quit', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('quit', [opts, ...args], resolve, reject)
      })
    }
  }

  async info(opts: Type, ...args: args): Promise<any>
  async info(...args: args): Promise<any>
  async info(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('info', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('info', [opts, ...args], resolve, reject)
      })
    }
  }

  async auth(opts: Type, ...args: args): Promise<any>
  async auth(...args: args): Promise<any>
  async auth(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('auth', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('auth', [opts, ...args], resolve, reject)
      })
    }
  }

  async client(opts: Type, ...args: args): Promise<any>
  async client(...args: args): Promise<any>
  async client(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('client', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('client', [opts, ...args], resolve, reject)
      })
    }
  }

  async hmset(opts: Type, ...args: args): Promise<any>
  async hmset(...args: args): Promise<any>
  async hmset(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hmset', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hmset', [opts, ...args], resolve, reject)
      })
    }
  }

  async subscribe(opts: Type, ...args: args): Promise<any>
  async subscribe(...args: args): Promise<any>
  async subscribe(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('subscribe', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('subscribe', [opts, ...args], resolve, reject)
      })
    }
  }

  async unsubscribe(opts: Type, ...args: args): Promise<any>
  async unsubscribe(...args: args): Promise<any>
  async unsubscribe(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('unsubscribe', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('unsubscribe', [opts, ...args], resolve, reject)
      })
    }
  }

  async psubscribe(opts: Type, ...args: args): Promise<any>
  async psubscribe(...args: args): Promise<any>
  async psubscribe(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('psubscribe', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('psubscribe', [opts, ...args], resolve, reject)
      })
    }
  }

  async punsubscribe(opts: Type, ...args: args): Promise<any>
  async punsubscribe(...args: args): Promise<any>
  async punsubscribe(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('punsubscribe', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('punsubscribe', [opts, ...args], resolve, reject)
      })
    }
  }

  async append(opts: Type, ...args: args): Promise<any>
  async append(...args: args): Promise<any>
  async append(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('append', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('append', [opts, ...args], resolve, reject)
      })
    }
  }

  async asking(opts: Type, ...args: args): Promise<any>
  async asking(...args: args): Promise<any>
  async asking(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('asking', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('asking', [opts, ...args], resolve, reject)
      })
    }
  }

  async bgrewriteaof(opts: Type, ...args: args): Promise<any>
  async bgrewriteaof(...args: args): Promise<any>
  async bgrewriteaof(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bgrewriteaof', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bgrewriteaof', [opts, ...args], resolve, reject)
      })
    }
  }

  async bgsave(opts: Type, ...args: args): Promise<any>
  async bgsave(...args: args): Promise<any>
  async bgsave(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bgsave', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bgsave', [opts, ...args], resolve, reject)
      })
    }
  }

  async bitcount(opts: Type, ...args: args): Promise<any>
  async bitcount(...args: args): Promise<any>
  async bitcount(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bitcount', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bitcount', [opts, ...args], resolve, reject)
      })
    }
  }

  async bitfield(opts: Type, ...args: args): Promise<any>
  async bitfield(...args: args): Promise<any>
  async bitfield(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bitfield', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bitfield', [opts, ...args], resolve, reject)
      })
    }
  }

  async bitop(opts: Type, ...args: args): Promise<any>
  async bitop(...args: args): Promise<any>
  async bitop(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bitop', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bitop', [opts, ...args], resolve, reject)
      })
    }
  }

  async bitpos(opts: Type, ...args: args): Promise<any>
  async bitpos(...args: args): Promise<any>
  async bitpos(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bitpos', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bitpos', [opts, ...args], resolve, reject)
      })
    }
  }

  async blpop(opts: Type, ...args: args): Promise<any>
  async blpop(...args: args): Promise<any>
  async blpop(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('blpop', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('blpop', [opts, ...args], resolve, reject)
      })
    }
  }

  async brpop(opts: Type, ...args: args): Promise<any>
  async brpop(...args: args): Promise<any>
  async brpop(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('brpop', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('brpop', [opts, ...args], resolve, reject)
      })
    }
  }

  async brpoplpush(opts: Type, ...args: args): Promise<any>
  async brpoplpush(...args: args): Promise<any>
  async brpoplpush(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('brpoplpush', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('brpoplpush', [opts, ...args], resolve, reject)
      })
    }
  }

  async bzpopmax(opts: Type, ...args: args): Promise<any>
  async bzpopmax(...args: args): Promise<any>
  async bzpopmax(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bzpopmax', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bzpopmax', [opts, ...args], resolve, reject)
      })
    }
  }

  async bzpopmin(opts: Type, ...args: args): Promise<any>
  async bzpopmin(...args: args): Promise<any>
  async bzpopmin(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bzpopmin', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('bzpopmin', [opts, ...args], resolve, reject)
      })
    }
  }

  async cluster(opts: Type, ...args: args): Promise<any>
  async cluster(...args: args): Promise<any>
  async cluster(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('cluster', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('cluster', [opts, ...args], resolve, reject)
      })
    }
  }

  async config(opts: Type, ...args: args): Promise<any>
  async config(...args: args): Promise<any>
  async config(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('config', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('config', [opts, ...args], resolve, reject)
      })
    }
  }

  async dbsize(opts: Type, ...args: args): Promise<any>
  async dbsize(...args: args): Promise<any>
  async dbsize(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('dbsize', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('dbsize', [opts, ...args], resolve, reject)
      })
    }
  }

  async debug(opts: Type, ...args: args): Promise<any>
  async debug(...args: args): Promise<any>
  async debug(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('debug', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('debug', [opts, ...args], resolve, reject)
      })
    }
  }

  async decr(opts: Type, ...args: args): Promise<any>
  async decr(...args: args): Promise<any>
  async decr(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('decr', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('decr', [opts, ...args], resolve, reject)
      })
    }
  }

  async decrby(opts: Type, ...args: args): Promise<any>
  async decrby(...args: args): Promise<any>
  async decrby(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('decrby', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('decrby', [opts, ...args], resolve, reject)
      })
    }
  }

  async del(opts: Type, ...args: args): Promise<any>
  async del(...args: args): Promise<any>
  async del(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('del', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('del', [opts, ...args], resolve, reject)
      })
    }
  }

  async discard(opts: Type, ...args: args): Promise<any>
  async discard(...args: args): Promise<any>
  async discard(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('discard', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('discard', [opts, ...args], resolve, reject)
      })
    }
  }

  async dump(opts: Type, ...args: args): Promise<any>
  async dump(...args: args): Promise<any>
  async dump(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('dump', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('dump', [opts, ...args], resolve, reject)
      })
    }
  }

  async echo(opts: Type, ...args: args): Promise<any>
  async echo(...args: args): Promise<any>
  async echo(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('echo', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('echo', [opts, ...args], resolve, reject)
      })
    }
  }

  async eval(opts: Type, ...args: args): Promise<any>
  async eval(...args: args): Promise<any>
  async eval(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('eval', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('eval', [opts, ...args], resolve, reject)
      })
    }
  }

  async evalsha(opts: Type, ...args: args): Promise<any>
  async evalsha(...args: args): Promise<any>
  async evalsha(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('evalsha', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('evalsha', [opts, ...args], resolve, reject)
      })
    }
  }

  async exec(opts: Type, ...args: args): Promise<any>
  async exec(...args: args): Promise<any>
  async exec(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('exec', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('exec', [opts, ...args], resolve, reject)
      })
    }
  }

  async exists(opts: Type, ...args: args): Promise<any>
  async exists(...args: args): Promise<any>
  async exists(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('exists', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('exists', [opts, ...args], resolve, reject)
      })
    }
  }

  async expire(opts: Type, ...args: args): Promise<any>
  async expire(...args: args): Promise<any>
  async expire(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('expire', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('expire', [opts, ...args], resolve, reject)
      })
    }
  }

  async expireat(opts: Type, ...args: args): Promise<any>
  async expireat(...args: args): Promise<any>
  async expireat(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('expireat', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('expireat', [opts, ...args], resolve, reject)
      })
    }
  }

  async flushall(opts: Type, ...args: args): Promise<any>
  async flushall(...args: args): Promise<any>
  async flushall(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('flushall', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('flushall', [opts, ...args], resolve, reject)
      })
    }
  }

  async flushdb(opts: Type, ...args: args): Promise<any>
  async flushdb(...args: args): Promise<any>
  async flushdb(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('flushdb', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('flushdb', [opts, ...args], resolve, reject)
      })
    }
  }

  async geoadd(opts: Type, ...args: args): Promise<any>
  async geoadd(...args: args): Promise<any>
  async geoadd(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('geoadd', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('geoadd', [opts, ...args], resolve, reject)
      })
    }
  }

  async geodist(opts: Type, ...args: args): Promise<any>
  async geodist(...args: args): Promise<any>
  async geodist(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('geodist', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('geodist', [opts, ...args], resolve, reject)
      })
    }
  }

  async geohash(opts: Type, ...args: args): Promise<any>
  async geohash(...args: args): Promise<any>
  async geohash(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('geohash', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('geohash', [opts, ...args], resolve, reject)
      })
    }
  }

  async geopos(opts: Type, ...args: args): Promise<any>
  async geopos(...args: args): Promise<any>
  async geopos(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('geopos', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('geopos', [opts, ...args], resolve, reject)
      })
    }
  }

  async georadius(opts: Type, ...args: args): Promise<any>
  async georadius(...args: args): Promise<any>
  async georadius(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('georadius', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('georadius', [opts, ...args], resolve, reject)
      })
    }
  }

  async georadiusbymember(opts: Type, ...args: args): Promise<any>
  async georadiusbymember(...args: args): Promise<any>
  async georadiusbymember(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('georadiusbymember', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue(
          'georadiusbymember',
          [opts, ...args],
          resolve,
          reject
        )
      })
    }
  }

  async get(opts: Type, ...args: args): Promise<any>
  async get(...args: args): Promise<any>
  async get(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('get', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('get', [opts, ...args], resolve, reject)
      })
    }
  }

  async getbit(opts: Type, ...args: args): Promise<any>
  async getbit(...args: args): Promise<any>
  async getbit(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('getbit', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('getbit', [opts, ...args], resolve, reject)
      })
    }
  }

  async getrange(opts: Type, ...args: args): Promise<any>
  async getrange(...args: args): Promise<any>
  async getrange(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('getrange', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('getrange', [opts, ...args], resolve, reject)
      })
    }
  }

  async getset(opts: Type, ...args: args): Promise<any>
  async getset(...args: args): Promise<any>
  async getset(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('getset', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('getset', [opts, ...args], resolve, reject)
      })
    }
  }

  async hdel(opts: Type, ...args: args): Promise<any>
  async hdel(...args: args): Promise<any>
  async hdel(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hdel', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hdel', [opts, ...args], resolve, reject)
      })
    }
  }

  async hexists(opts: Type, ...args: args): Promise<any>
  async hexists(...args: args): Promise<any>
  async hexists(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hexists', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hexists', [opts, ...args], resolve, reject)
      })
    }
  }

  async hget(opts: Type, ...args: args): Promise<any>
  async hget(...args: args): Promise<any>
  async hget(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hget', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hget', [opts, ...args], resolve, reject)
      })
    }
  }

  async hgetall(opts: Type, ...args: args): Promise<any>
  async hgetall(...args: args): Promise<any>
  async hgetall(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hgetall', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hgetall', [opts, ...args], resolve, reject)
      })
    }
  }

  async hincrby(opts: Type, ...args: args): Promise<any>
  async hincrby(...args: args): Promise<any>
  async hincrby(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hincrby', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hincrby', [opts, ...args], resolve, reject)
      })
    }
  }

  async hincrbyfloat(opts: Type, ...args: args): Promise<any>
  async hincrbyfloat(...args: args): Promise<any>
  async hincrbyfloat(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hincrbyfloat', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hincrbyfloat', [opts, ...args], resolve, reject)
      })
    }
  }

  async hkeys(opts: Type, ...args: args): Promise<any>
  async hkeys(...args: args): Promise<any>
  async hkeys(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hkeys', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hkeys', [opts, ...args], resolve, reject)
      })
    }
  }

  async hlen(opts: Type, ...args: args): Promise<any>
  async hlen(...args: args): Promise<any>
  async hlen(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hlen', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hlen', [opts, ...args], resolve, reject)
      })
    }
  }

  async hmget(opts: Type, ...args: args): Promise<any>
  async hmget(...args: args): Promise<any>
  async hmget(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hmget', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hmget', [opts, ...args], resolve, reject)
      })
    }
  }

  async hscan(opts: Type, ...args: args): Promise<any>
  async hscan(...args: args): Promise<any>
  async hscan(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hscan', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hscan', [opts, ...args], resolve, reject)
      })
    }
  }

  async hset(opts: Type, ...args: args): Promise<any>
  async hset(...args: args): Promise<any>
  async hset(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hset', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hset', [opts, ...args], resolve, reject)
      })
    }
  }

  async hsetnx(opts: Type, ...args: args): Promise<any>
  async hsetnx(...args: args): Promise<any>
  async hsetnx(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hsetnx', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hsetnx', [opts, ...args], resolve, reject)
      })
    }
  }

  async hstrlen(opts: Type, ...args: args): Promise<any>
  async hstrlen(...args: args): Promise<any>
  async hstrlen(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hstrlen', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hstrlen', [opts, ...args], resolve, reject)
      })
    }
  }

  async hvals(opts: Type, ...args: args): Promise<any>
  async hvals(...args: args): Promise<any>
  async hvals(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hvals', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('hvals', [opts, ...args], resolve, reject)
      })
    }
  }

  async incr(opts: Type, ...args: args): Promise<any>
  async incr(...args: args): Promise<any>
  async incr(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('incr', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('incr', [opts, ...args], resolve, reject)
      })
    }
  }

  async incrby(opts: Type, ...args: args): Promise<any>
  async incrby(...args: args): Promise<any>
  async incrby(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('incrby', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('incrby', [opts, ...args], resolve, reject)
      })
    }
  }

  async incrbyfloat(opts: Type, ...args: args): Promise<any>
  async incrbyfloat(...args: args): Promise<any>
  async incrbyfloat(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('incrbyfloat', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('incrbyfloat', [opts, ...args], resolve, reject)
      })
    }
  }

  async keys(opts: Type, ...args: args): Promise<any>
  async keys(...args: args): Promise<any>
  async keys(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('keys', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('keys', [opts, ...args], resolve, reject)
      })
    }
  }

  async lastsave(opts: Type, ...args: args): Promise<any>
  async lastsave(...args: args): Promise<any>
  async lastsave(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lastsave', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lastsave', [opts, ...args], resolve, reject)
      })
    }
  }

  async latency(opts: Type, ...args: args): Promise<any>
  async latency(...args: args): Promise<any>
  async latency(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('latency', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('latency', [opts, ...args], resolve, reject)
      })
    }
  }

  async lindex(opts: Type, ...args: args): Promise<any>
  async lindex(...args: args): Promise<any>
  async lindex(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lindex', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lindex', [opts, ...args], resolve, reject)
      })
    }
  }

  async linsert(opts: Type, ...args: args): Promise<any>
  async linsert(...args: args): Promise<any>
  async linsert(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('linsert', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('linsert', [opts, ...args], resolve, reject)
      })
    }
  }

  async llen(opts: Type, ...args: args): Promise<any>
  async llen(...args: args): Promise<any>
  async llen(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('llen', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('llen', [opts, ...args], resolve, reject)
      })
    }
  }

  async lolwut(opts: Type, ...args: args): Promise<any>
  async lolwut(...args: args): Promise<any>
  async lolwut(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lolwut', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lolwut', [opts, ...args], resolve, reject)
      })
    }
  }

  async lpop(opts: Type, ...args: args): Promise<any>
  async lpop(...args: args): Promise<any>
  async lpop(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lpop', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lpop', [opts, ...args], resolve, reject)
      })
    }
  }

  async lpush(opts: Type, ...args: args): Promise<any>
  async lpush(...args: args): Promise<any>
  async lpush(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lpush', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lpush', [opts, ...args], resolve, reject)
      })
    }
  }

  async lpushx(opts: Type, ...args: args): Promise<any>
  async lpushx(...args: args): Promise<any>
  async lpushx(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lpushx', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lpushx', [opts, ...args], resolve, reject)
      })
    }
  }

  async lrange(opts: Type, ...args: args): Promise<any>
  async lrange(...args: args): Promise<any>
  async lrange(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lrange', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lrange', [opts, ...args], resolve, reject)
      })
    }
  }

  async lrem(opts: Type, ...args: args): Promise<any>
  async lrem(...args: args): Promise<any>
  async lrem(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lrem', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lrem', [opts, ...args], resolve, reject)
      })
    }
  }

  async lset(opts: Type, ...args: args): Promise<any>
  async lset(...args: args): Promise<any>
  async lset(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lset', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('lset', [opts, ...args], resolve, reject)
      })
    }
  }

  async ltrim(opts: Type, ...args: args): Promise<any>
  async ltrim(...args: args): Promise<any>
  async ltrim(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('ltrim', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('ltrim', [opts, ...args], resolve, reject)
      })
    }
  }

  async memory(opts: Type, ...args: args): Promise<any>
  async memory(...args: args): Promise<any>
  async memory(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('memory', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('memory', [opts, ...args], resolve, reject)
      })
    }
  }

  async mget(opts: Type, ...args: args): Promise<any>
  async mget(...args: args): Promise<any>
  async mget(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('mget', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('mget', [opts, ...args], resolve, reject)
      })
    }
  }

  async migrate(opts: Type, ...args: args): Promise<any>
  async migrate(...args: args): Promise<any>
  async migrate(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('migrate', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('migrate', [opts, ...args], resolve, reject)
      })
    }
  }

  async module(opts: Type, ...args: args): Promise<any>
  async module(...args: args): Promise<any>
  async module(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('module', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('module', [opts, ...args], resolve, reject)
      })
    }
  }

  async move(opts: Type, ...args: args): Promise<any>
  async move(...args: args): Promise<any>
  async move(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('move', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('move', [opts, ...args], resolve, reject)
      })
    }
  }

  async mset(opts: Type, ...args: args): Promise<any>
  async mset(...args: args): Promise<any>
  async mset(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('mset', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('mset', [opts, ...args], resolve, reject)
      })
    }
  }

  async msetnx(opts: Type, ...args: args): Promise<any>
  async msetnx(...args: args): Promise<any>
  async msetnx(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('msetnx', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('msetnx', [opts, ...args], resolve, reject)
      })
    }
  }

  async object(opts: Type, ...args: args): Promise<any>
  async object(...args: args): Promise<any>
  async object(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('object', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('object', [opts, ...args], resolve, reject)
      })
    }
  }

  async persist(opts: Type, ...args: args): Promise<any>
  async persist(...args: args): Promise<any>
  async persist(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('persist', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('persist', [opts, ...args], resolve, reject)
      })
    }
  }

  async pexpire(opts: Type, ...args: args): Promise<any>
  async pexpire(...args: args): Promise<any>
  async pexpire(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pexpire', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pexpire', [opts, ...args], resolve, reject)
      })
    }
  }

  async pexpireat(opts: Type, ...args: args): Promise<any>
  async pexpireat(...args: args): Promise<any>
  async pexpireat(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pexpireat', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pexpireat', [opts, ...args], resolve, reject)
      })
    }
  }

  async pfadd(opts: Type, ...args: args): Promise<any>
  async pfadd(...args: args): Promise<any>
  async pfadd(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pfadd', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pfadd', [opts, ...args], resolve, reject)
      })
    }
  }

  async pfcount(opts: Type, ...args: args): Promise<any>
  async pfcount(...args: args): Promise<any>
  async pfcount(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pfcount', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pfcount', [opts, ...args], resolve, reject)
      })
    }
  }

  async pfdebug(opts: Type, ...args: args): Promise<any>
  async pfdebug(...args: args): Promise<any>
  async pfdebug(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pfdebug', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pfdebug', [opts, ...args], resolve, reject)
      })
    }
  }

  async pfmerge(opts: Type, ...args: args): Promise<any>
  async pfmerge(...args: args): Promise<any>
  async pfmerge(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pfmerge', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pfmerge', [opts, ...args], resolve, reject)
      })
    }
  }

  async pfselftest(opts: Type, ...args: args): Promise<any>
  async pfselftest(...args: args): Promise<any>
  async pfselftest(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pfselftest', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pfselftest', [opts, ...args], resolve, reject)
      })
    }
  }

  async ping(opts: Type, ...args: args): Promise<any>
  async ping(...args: args): Promise<any>
  async ping(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('ping', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('ping', [opts, ...args], resolve, reject)
      })
    }
  }

  async post(opts: Type, ...args: args): Promise<any>
  async post(...args: args): Promise<any>
  async post(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('post', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('post', [opts, ...args], resolve, reject)
      })
    }
  }

  async psetex(opts: Type, ...args: args): Promise<any>
  async psetex(...args: args): Promise<any>
  async psetex(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('psetex', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('psetex', [opts, ...args], resolve, reject)
      })
    }
  }

  async psync(opts: Type, ...args: args): Promise<any>
  async psync(...args: args): Promise<any>
  async psync(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('psync', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('psync', [opts, ...args], resolve, reject)
      })
    }
  }

  async pttl(opts: Type, ...args: args): Promise<any>
  async pttl(...args: args): Promise<any>
  async pttl(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pttl', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pttl', [opts, ...args], resolve, reject)
      })
    }
  }

  async publish(opts: Type, ...args: args): Promise<any>
  async publish(...args: args): Promise<any>
  async publish(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('publish', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('publish', [opts, ...args], resolve, reject)
      })
    }
  }

  async pubsub(opts: Type, ...args: args): Promise<any>
  async pubsub(...args: args): Promise<any>
  async pubsub(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pubsub', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('pubsub', [opts, ...args], resolve, reject)
      })
    }
  }

  async randomkey(opts: Type, ...args: args): Promise<any>
  async randomkey(...args: args): Promise<any>
  async randomkey(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('randomkey', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('randomkey', [opts, ...args], resolve, reject)
      })
    }
  }

  async readonly(opts: Type, ...args: args): Promise<any>
  async readonly(...args: args): Promise<any>
  async readonly(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('readonly', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('readonly', [opts, ...args], resolve, reject)
      })
    }
  }

  async readwrite(opts: Type, ...args: args): Promise<any>
  async readwrite(...args: args): Promise<any>
  async readwrite(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('readwrite', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('readwrite', [opts, ...args], resolve, reject)
      })
    }
  }

  async rename(opts: Type, ...args: args): Promise<any>
  async rename(...args: args): Promise<any>
  async rename(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('rename', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('rename', [opts, ...args], resolve, reject)
      })
    }
  }

  async renamenx(opts: Type, ...args: args): Promise<any>
  async renamenx(...args: args): Promise<any>
  async renamenx(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('renamenx', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('renamenx', [opts, ...args], resolve, reject)
      })
    }
  }

  async replconf(opts: Type, ...args: args): Promise<any>
  async replconf(...args: args): Promise<any>
  async replconf(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('replconf', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('replconf', [opts, ...args], resolve, reject)
      })
    }
  }

  async replicaof(opts: Type, ...args: args): Promise<any>
  async replicaof(...args: args): Promise<any>
  async replicaof(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('replicaof', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('replicaof', [opts, ...args], resolve, reject)
      })
    }
  }

  async restore(opts: Type, ...args: args): Promise<any>
  async restore(...args: args): Promise<any>
  async restore(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('restore', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('restore', [opts, ...args], resolve, reject)
      })
    }
  }

  async role(opts: Type, ...args: args): Promise<any>
  async role(...args: args): Promise<any>
  async role(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('role', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('role', [opts, ...args], resolve, reject)
      })
    }
  }

  async rpop(opts: Type, ...args: args): Promise<any>
  async rpop(...args: args): Promise<any>
  async rpop(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('rpop', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('rpop', [opts, ...args], resolve, reject)
      })
    }
  }

  async rpoplpush(opts: Type, ...args: args): Promise<any>
  async rpoplpush(...args: args): Promise<any>
  async rpoplpush(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('rpoplpush', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('rpoplpush', [opts, ...args], resolve, reject)
      })
    }
  }

  async rpush(opts: Type, ...args: args): Promise<any>
  async rpush(...args: args): Promise<any>
  async rpush(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('rpush', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('rpush', [opts, ...args], resolve, reject)
      })
    }
  }

  async rpushx(opts: Type, ...args: args): Promise<any>
  async rpushx(...args: args): Promise<any>
  async rpushx(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('rpushx', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('rpushx', [opts, ...args], resolve, reject)
      })
    }
  }

  async sadd(opts: Type, ...args: args): Promise<any>
  async sadd(...args: args): Promise<any>
  async sadd(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sadd', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sadd', [opts, ...args], resolve, reject)
      })
    }
  }

  async save(opts: Type, ...args: args): Promise<any>
  async save(...args: args): Promise<any>
  async save(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('save', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('save', [opts, ...args], resolve, reject)
      })
    }
  }

  async scan(opts: Type, ...args: args): Promise<any>
  async scan(...args: args): Promise<any>
  async scan(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('scan', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('scan', [opts, ...args], resolve, reject)
      })
    }
  }

  async scard(opts: Type, ...args: args): Promise<any>
  async scard(...args: args): Promise<any>
  async scard(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('scard', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('scard', [opts, ...args], resolve, reject)
      })
    }
  }

  async script(opts: Type, ...args: args): Promise<any>
  async script(...args: args): Promise<any>
  async script(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('script', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('script', [opts, ...args], resolve, reject)
      })
    }
  }

  async sdiff(opts: Type, ...args: args): Promise<any>
  async sdiff(...args: args): Promise<any>
  async sdiff(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sdiff', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sdiff', [opts, ...args], resolve, reject)
      })
    }
  }

  async sdiffstore(opts: Type, ...args: args): Promise<any>
  async sdiffstore(...args: args): Promise<any>
  async sdiffstore(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sdiffstore', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sdiffstore', [opts, ...args], resolve, reject)
      })
    }
  }

  async set(opts: Type, ...args: args): Promise<any>
  async set(...args: args): Promise<any>
  async set(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('set', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('set', [opts, ...args], resolve, reject)
      })
    }
  }

  async setbit(opts: Type, ...args: args): Promise<any>
  async setbit(...args: args): Promise<any>
  async setbit(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('setbit', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('setbit', [opts, ...args], resolve, reject)
      })
    }
  }

  async setex(opts: Type, ...args: args): Promise<any>
  async setex(...args: args): Promise<any>
  async setex(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('setex', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('setex', [opts, ...args], resolve, reject)
      })
    }
  }

  async setnx(opts: Type, ...args: args): Promise<any>
  async setnx(...args: args): Promise<any>
  async setnx(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('setnx', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('setnx', [opts, ...args], resolve, reject)
      })
    }
  }

  async setrange(opts: Type, ...args: args): Promise<any>
  async setrange(...args: args): Promise<any>
  async setrange(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('setrange', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('setrange', [opts, ...args], resolve, reject)
      })
    }
  }

  async shutdown(opts: Type, ...args: args): Promise<any>
  async shutdown(...args: args): Promise<any>
  async shutdown(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('shutdown', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('shutdown', [opts, ...args], resolve, reject)
      })
    }
  }

  async sinter(opts: Type, ...args: args): Promise<any>
  async sinter(...args: args): Promise<any>
  async sinter(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sinter', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sinter', [opts, ...args], resolve, reject)
      })
    }
  }

  async sinterstore(opts: Type, ...args: args): Promise<any>
  async sinterstore(...args: args): Promise<any>
  async sinterstore(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sinterstore', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sinterstore', [opts, ...args], resolve, reject)
      })
    }
  }

  async sismember(opts: Type, ...args: args): Promise<any>
  async sismember(...args: args): Promise<any>
  async sismember(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sismember', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sismember', [opts, ...args], resolve, reject)
      })
    }
  }

  async slaveof(opts: Type, ...args: args): Promise<any>
  async slaveof(...args: args): Promise<any>
  async slaveof(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('slaveof', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('slaveof', [opts, ...args], resolve, reject)
      })
    }
  }

  async slowlog(opts: Type, ...args: args): Promise<any>
  async slowlog(...args: args): Promise<any>
  async slowlog(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('slowlog', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('slowlog', [opts, ...args], resolve, reject)
      })
    }
  }

  async smembers(opts: Type, ...args: args): Promise<any>
  async smembers(...args: args): Promise<any>
  async smembers(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('smembers', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('smembers', [opts, ...args], resolve, reject)
      })
    }
  }

  async smove(opts: Type, ...args: args): Promise<any>
  async smove(...args: args): Promise<any>
  async smove(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('smove', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('smove', [opts, ...args], resolve, reject)
      })
    }
  }

  async sort(opts: Type, ...args: args): Promise<any>
  async sort(...args: args): Promise<any>
  async sort(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sort', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sort', [opts, ...args], resolve, reject)
      })
    }
  }

  async spop(opts: Type, ...args: args): Promise<any>
  async spop(...args: args): Promise<any>
  async spop(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('spop', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('spop', [opts, ...args], resolve, reject)
      })
    }
  }

  async srandmember(opts: Type, ...args: args): Promise<any>
  async srandmember(...args: args): Promise<any>
  async srandmember(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('srandmember', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('srandmember', [opts, ...args], resolve, reject)
      })
    }
  }

  async srem(opts: Type, ...args: args): Promise<any>
  async srem(...args: args): Promise<any>
  async srem(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('srem', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('srem', [opts, ...args], resolve, reject)
      })
    }
  }

  async sscan(opts: Type, ...args: args): Promise<any>
  async sscan(...args: args): Promise<any>
  async sscan(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sscan', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sscan', [opts, ...args], resolve, reject)
      })
    }
  }

  async strlen(opts: Type, ...args: args): Promise<any>
  async strlen(...args: args): Promise<any>
  async strlen(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('strlen', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('strlen', [opts, ...args], resolve, reject)
      })
    }
  }

  async substr(opts: Type, ...args: args): Promise<any>
  async substr(...args: args): Promise<any>
  async substr(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('substr', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('substr', [opts, ...args], resolve, reject)
      })
    }
  }

  async sunion(opts: Type, ...args: args): Promise<any>
  async sunion(...args: args): Promise<any>
  async sunion(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sunion', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sunion', [opts, ...args], resolve, reject)
      })
    }
  }

  async sunionstore(opts: Type, ...args: args): Promise<any>
  async sunionstore(...args: args): Promise<any>
  async sunionstore(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sunionstore', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sunionstore', [opts, ...args], resolve, reject)
      })
    }
  }

  async swapdb(opts: Type, ...args: args): Promise<any>
  async swapdb(...args: args): Promise<any>
  async swapdb(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('swapdb', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('swapdb', [opts, ...args], resolve, reject)
      })
    }
  }

  async sync(opts: Type, ...args: args): Promise<any>
  async sync(...args: args): Promise<any>
  async sync(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sync', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('sync', [opts, ...args], resolve, reject)
      })
    }
  }

  async time(opts: Type, ...args: args): Promise<any>
  async time(...args: args): Promise<any>
  async time(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('time', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('time', [opts, ...args], resolve, reject)
      })
    }
  }

  async touch(opts: Type, ...args: args): Promise<any>
  async touch(...args: args): Promise<any>
  async touch(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('touch', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('touch', [opts, ...args], resolve, reject)
      })
    }
  }

  async ttl(opts: Type, ...args: args): Promise<any>
  async ttl(...args: args): Promise<any>
  async ttl(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('ttl', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('ttl', [opts, ...args], resolve, reject)
      })
    }
  }

  async type(opts: Type, ...args: args): Promise<any>
  async type(...args: args): Promise<any>
  async type(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('type', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('type', [opts, ...args], resolve, reject)
      })
    }
  }

  async unlink(opts: Type, ...args: args): Promise<any>
  async unlink(...args: args): Promise<any>
  async unlink(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('unlink', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('unlink', [opts, ...args], resolve, reject)
      })
    }
  }

  async unwatch(opts: Type, ...args: args): Promise<any>
  async unwatch(...args: args): Promise<any>
  async unwatch(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('unwatch', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('unwatch', [opts, ...args], resolve, reject)
      })
    }
  }

  async wait(opts: Type, ...args: args): Promise<any>
  async wait(...args: args): Promise<any>
  async wait(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('wait', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('wait', [opts, ...args], resolve, reject)
      })
    }
  }

  async watch(opts: Type, ...args: args): Promise<any>
  async watch(...args: args): Promise<any>
  async watch(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('watch', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('watch', [opts, ...args], resolve, reject)
      })
    }
  }

  async xack(opts: Type, ...args: args): Promise<any>
  async xack(...args: args): Promise<any>
  async xack(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xack', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xack', [opts, ...args], resolve, reject)
      })
    }
  }

  async xadd(opts: Type, ...args: args): Promise<any>
  async xadd(...args: args): Promise<any>
  async xadd(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xadd', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xadd', [opts, ...args], resolve, reject)
      })
    }
  }

  async xclaim(opts: Type, ...args: args): Promise<any>
  async xclaim(...args: args): Promise<any>
  async xclaim(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xclaim', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xclaim', [opts, ...args], resolve, reject)
      })
    }
  }

  async xdel(opts: Type, ...args: args): Promise<any>
  async xdel(...args: args): Promise<any>
  async xdel(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xdel', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xdel', [opts, ...args], resolve, reject)
      })
    }
  }

  async xgroup(opts: Type, ...args: args): Promise<any>
  async xgroup(...args: args): Promise<any>
  async xgroup(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xgroup', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xgroup', [opts, ...args], resolve, reject)
      })
    }
  }

  async xinfo(opts: Type, ...args: args): Promise<any>
  async xinfo(...args: args): Promise<any>
  async xinfo(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xinfo', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xinfo', [opts, ...args], resolve, reject)
      })
    }
  }

  async xlen(opts: Type, ...args: args): Promise<any>
  async xlen(...args: args): Promise<any>
  async xlen(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xlen', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xlen', [opts, ...args], resolve, reject)
      })
    }
  }

  async xpending(opts: Type, ...args: args): Promise<any>
  async xpending(...args: args): Promise<any>
  async xpending(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xpending', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xpending', [opts, ...args], resolve, reject)
      })
    }
  }

  async xrange(opts: Type, ...args: args): Promise<any>
  async xrange(...args: args): Promise<any>
  async xrange(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xrange', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xrange', [opts, ...args], resolve, reject)
      })
    }
  }

  async xread(opts: Type, ...args: args): Promise<any>
  async xread(...args: args): Promise<any>
  async xread(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xread', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xread', [opts, ...args], resolve, reject)
      })
    }
  }

  async xreadgroup(opts: Type, ...args: args): Promise<any>
  async xreadgroup(...args: args): Promise<any>
  async xreadgroup(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xreadgroup', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xreadgroup', [opts, ...args], resolve, reject)
      })
    }
  }

  async xrevrange(opts: Type, ...args: args): Promise<any>
  async xrevrange(...args: args): Promise<any>
  async xrevrange(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xrevrange', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xrevrange', [opts, ...args], resolve, reject)
      })
    }
  }

  async xsetid(opts: Type, ...args: args): Promise<any>
  async xsetid(...args: args): Promise<any>
  async xsetid(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xsetid', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xsetid', [opts, ...args], resolve, reject)
      })
    }
  }

  async xtrim(opts: Type, ...args: args): Promise<any>
  async xtrim(...args: args): Promise<any>
  async xtrim(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xtrim', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('xtrim', [opts, ...args], resolve, reject)
      })
    }
  }

  async zadd(opts: Type, ...args: args): Promise<any>
  async zadd(...args: args): Promise<any>
  async zadd(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zadd', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zadd', [opts, ...args], resolve, reject)
      })
    }
  }

  async zcard(opts: Type, ...args: args): Promise<any>
  async zcard(...args: args): Promise<any>
  async zcard(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zcard', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zcard', [opts, ...args], resolve, reject)
      })
    }
  }

  async zcount(opts: Type, ...args: args): Promise<any>
  async zcount(...args: args): Promise<any>
  async zcount(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zcount', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zcount', [opts, ...args], resolve, reject)
      })
    }
  }

  async zincrby(opts: Type, ...args: args): Promise<any>
  async zincrby(...args: args): Promise<any>
  async zincrby(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zincrby', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zincrby', [opts, ...args], resolve, reject)
      })
    }
  }

  async zinterstore(opts: Type, ...args: args): Promise<any>
  async zinterstore(...args: args): Promise<any>
  async zinterstore(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zinterstore', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zinterstore', [opts, ...args], resolve, reject)
      })
    }
  }

  async zlexcount(opts: Type, ...args: args): Promise<any>
  async zlexcount(...args: args): Promise<any>
  async zlexcount(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zlexcount', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zlexcount', [opts, ...args], resolve, reject)
      })
    }
  }

  async zpopmax(opts: Type, ...args: args): Promise<any>
  async zpopmax(...args: args): Promise<any>
  async zpopmax(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zpopmax', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zpopmax', [opts, ...args], resolve, reject)
      })
    }
  }

  async zpopmin(opts: Type, ...args: args): Promise<any>
  async zpopmin(...args: args): Promise<any>
  async zpopmin(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zpopmin', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zpopmin', [opts, ...args], resolve, reject)
      })
    }
  }

  async zrange(opts: Type, ...args: args): Promise<any>
  async zrange(...args: args): Promise<any>
  async zrange(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrange', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrange', [opts, ...args], resolve, reject)
      })
    }
  }

  async zrangebylex(opts: Type, ...args: args): Promise<any>
  async zrangebylex(...args: args): Promise<any>
  async zrangebylex(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrangebylex', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrangebylex', [opts, ...args], resolve, reject)
      })
    }
  }

  async zrangebyscore(opts: Type, ...args: args): Promise<any>
  async zrangebyscore(...args: args): Promise<any>
  async zrangebyscore(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrangebyscore', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue(
          'zrangebyscore',
          [opts, ...args],
          resolve,
          reject
        )
      })
    }
  }

  async zrank(opts: Type, ...args: args): Promise<any>
  async zrank(...args: args): Promise<any>
  async zrank(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrank', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrank', [opts, ...args], resolve, reject)
      })
    }
  }

  async zrem(opts: Type, ...args: args): Promise<any>
  async zrem(...args: args): Promise<any>
  async zrem(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrem', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrem', [opts, ...args], resolve, reject)
      })
    }
  }

  async zremrangebylex(opts: Type, ...args: args): Promise<any>
  async zremrangebylex(...args: args): Promise<any>
  async zremrangebylex(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zremrangebylex', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue(
          'zremrangebylex',
          [opts, ...args],
          resolve,
          reject
        )
      })
    }
  }

  async zremrangebyrank(opts: Type, ...args: args): Promise<any>
  async zremrangebyrank(...args: args): Promise<any>
  async zremrangebyrank(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zremrangebyrank', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue(
          'zremrangebyrank',
          [opts, ...args],
          resolve,
          reject
        )
      })
    }
  }

  async zremrangebyscore(opts: Type, ...args: args): Promise<any>
  async zremrangebyscore(...args: args): Promise<any>
  async zremrangebyscore(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zremrangebyscore', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue(
          'zremrangebyscore',
          [opts, ...args],
          resolve,
          reject
        )
      })
    }
  }

  async zrevrange(opts: Type, ...args: args): Promise<any>
  async zrevrange(...args: args): Promise<any>
  async zrevrange(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrevrange', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrevrange', [opts, ...args], resolve, reject)
      })
    }
  }

  async zrevrangebylex(opts: Type, ...args: args): Promise<any>
  async zrevrangebylex(...args: args): Promise<any>
  async zrevrangebylex(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrevrangebylex', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue(
          'zrevrangebylex',
          [opts, ...args],
          resolve,
          reject
        )
      })
    }
  }

  async zrevrangebyscore(opts: Type, ...args: args): Promise<any>
  async zrevrangebyscore(...args: args): Promise<any>
  async zrevrangebyscore(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrevrangebyscore', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue(
          'zrevrangebyscore',
          [opts, ...args],
          resolve,
          reject
        )
      })
    }
  }

  async zrevrank(opts: Type, ...args: args): Promise<any>
  async zrevrank(...args: args): Promise<any>
  async zrevrank(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrevrank', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zrevrank', [opts, ...args], resolve, reject)
      })
    }
  }

  async zscan(opts: Type, ...args: args): Promise<any>
  async zscan(...args: args): Promise<any>
  async zscan(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zscan', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zscan', [opts, ...args], resolve, reject)
      })
    }
  }

  async zscore(opts: Type, ...args: args): Promise<any>
  async zscore(...args: args): Promise<any>
  async zscore(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zscore', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zscore', [opts, ...args], resolve, reject)
      })
    }
  }

  async zunionstore(opts: Type, ...args: args): Promise<any>
  async zunionstore(...args: args): Promise<any>
  async zunionstore(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zunionstore', args, resolve, reject, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue('zunionstore', [opts, ...args], resolve, reject)
      })
    }
  }
}

export default RedisMethods
