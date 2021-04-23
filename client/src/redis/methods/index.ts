
type args = (string | number | Buffer)[]
import { RedisCommand } from '../types'
import { ServerSelector } from '../../types'

abstract class RedisMethods {
  abstract addCommandToQueue(
    redisCommand: RedisCommand,
    opts?: ServerSelector
  ): void
  async command(opts: ServerSelector, key: string, ...args: args): Promise<any>
  async command(key: string, ...args: args): Promise<any>
  async command(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        const [key, ...commandArgs] = args
        this.addCommandToQueue({ command: <string>key, args: commandArgs, resolve, reject }, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue({ command: opts, args, resolve, reject })
      })
    }
  }
  
async multi(opts: ServerSelector, ...args: args): Promise<any>
async multi(...args: args): Promise<any>
async multi(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'multi', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'multi', args: [opts, ...args], resolve, reject })
    })
  }
}


async batch(opts: ServerSelector, ...args: args): Promise<any>
async batch(...args: args): Promise<any>
async batch(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'batch', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'batch', args: [opts, ...args], resolve, reject })
    })
  }
}


async select(opts: ServerSelector, ...args: args): Promise<any>
async select(...args: args): Promise<any>
async select(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'select', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'select', args: [opts, ...args], resolve, reject })
    })
  }
}


async monitor(opts: ServerSelector, ...args: args): Promise<any>
async monitor(...args: args): Promise<any>
async monitor(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'monitor', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'monitor', args: [opts, ...args], resolve, reject })
    })
  }
}


async quit(opts: ServerSelector, ...args: args): Promise<any>
async quit(...args: args): Promise<any>
async quit(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'quit', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'quit', args: [opts, ...args], resolve, reject })
    })
  }
}


async info(opts: ServerSelector, ...args: args): Promise<any>
async info(...args: args): Promise<any>
async info(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'info', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'info', args: [opts, ...args], resolve, reject })
    })
  }
}


async auth(opts: ServerSelector, ...args: args): Promise<any>
async auth(...args: args): Promise<any>
async auth(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'auth', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'auth', args: [opts, ...args], resolve, reject })
    })
  }
}


async client(opts: ServerSelector, ...args: args): Promise<any>
async client(...args: args): Promise<any>
async client(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'client', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'client', args: [opts, ...args], resolve, reject })
    })
  }
}


async hmset(opts: ServerSelector, ...args: args): Promise<any>
async hmset(...args: args): Promise<any>
async hmset(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hmset', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hmset', args: [opts, ...args], resolve, reject })
    })
  }
}


async subscribe(opts: ServerSelector, ...args: args): Promise<any>
async subscribe(...args: args): Promise<any>
async subscribe(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'subscribe', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'subscribe', args: [opts, ...args], resolve, reject })
    })
  }
}


async unsubscribe(opts: ServerSelector, ...args: args): Promise<any>
async unsubscribe(...args: args): Promise<any>
async unsubscribe(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'unsubscribe', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'unsubscribe', args: [opts, ...args], resolve, reject })
    })
  }
}


async psubscribe(opts: ServerSelector, ...args: args): Promise<any>
async psubscribe(...args: args): Promise<any>
async psubscribe(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'psubscribe', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'psubscribe', args: [opts, ...args], resolve, reject })
    })
  }
}


async punsubscribe(opts: ServerSelector, ...args: args): Promise<any>
async punsubscribe(...args: args): Promise<any>
async punsubscribe(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'punsubscribe', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'punsubscribe', args: [opts, ...args], resolve, reject })
    })
  }
}


async acl(opts: ServerSelector, ...args: args): Promise<any>
async acl(...args: args): Promise<any>
async acl(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'acl', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'acl', args: [opts, ...args], resolve, reject })
    })
  }
}


async append(opts: ServerSelector, ...args: args): Promise<any>
async append(...args: args): Promise<any>
async append(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'append', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'append', args: [opts, ...args], resolve, reject })
    })
  }
}


async asking(opts: ServerSelector, ...args: args): Promise<any>
async asking(...args: args): Promise<any>
async asking(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'asking', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'asking', args: [opts, ...args], resolve, reject })
    })
  }
}


async bgrewriteaof(opts: ServerSelector, ...args: args): Promise<any>
async bgrewriteaof(...args: args): Promise<any>
async bgrewriteaof(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bgrewriteaof', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bgrewriteaof', args: [opts, ...args], resolve, reject })
    })
  }
}


async bgsave(opts: ServerSelector, ...args: args): Promise<any>
async bgsave(...args: args): Promise<any>
async bgsave(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bgsave', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bgsave', args: [opts, ...args], resolve, reject })
    })
  }
}


async bitcount(opts: ServerSelector, ...args: args): Promise<any>
async bitcount(...args: args): Promise<any>
async bitcount(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bitcount', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bitcount', args: [opts, ...args], resolve, reject })
    })
  }
}


async bitfield(opts: ServerSelector, ...args: args): Promise<any>
async bitfield(...args: args): Promise<any>
async bitfield(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bitfield', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bitfield', args: [opts, ...args], resolve, reject })
    })
  }
}


async bitfield_ro(opts: ServerSelector, ...args: args): Promise<any>
async bitfield_ro(...args: args): Promise<any>
async bitfield_ro(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bitfield_ro', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bitfield_ro', args: [opts, ...args], resolve, reject })
    })
  }
}


async bitop(opts: ServerSelector, ...args: args): Promise<any>
async bitop(...args: args): Promise<any>
async bitop(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bitop', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bitop', args: [opts, ...args], resolve, reject })
    })
  }
}


async bitpos(opts: ServerSelector, ...args: args): Promise<any>
async bitpos(...args: args): Promise<any>
async bitpos(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bitpos', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bitpos', args: [opts, ...args], resolve, reject })
    })
  }
}


async blmove(opts: ServerSelector, ...args: args): Promise<any>
async blmove(...args: args): Promise<any>
async blmove(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'blmove', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'blmove', args: [opts, ...args], resolve, reject })
    })
  }
}


async blpop(opts: ServerSelector, ...args: args): Promise<any>
async blpop(...args: args): Promise<any>
async blpop(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'blpop', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'blpop', args: [opts, ...args], resolve, reject })
    })
  }
}


async brpop(opts: ServerSelector, ...args: args): Promise<any>
async brpop(...args: args): Promise<any>
async brpop(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'brpop', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'brpop', args: [opts, ...args], resolve, reject })
    })
  }
}


async brpoplpush(opts: ServerSelector, ...args: args): Promise<any>
async brpoplpush(...args: args): Promise<any>
async brpoplpush(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'brpoplpush', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'brpoplpush', args: [opts, ...args], resolve, reject })
    })
  }
}


async bzpopmax(opts: ServerSelector, ...args: args): Promise<any>
async bzpopmax(...args: args): Promise<any>
async bzpopmax(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bzpopmax', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bzpopmax', args: [opts, ...args], resolve, reject })
    })
  }
}


async bzpopmin(opts: ServerSelector, ...args: args): Promise<any>
async bzpopmin(...args: args): Promise<any>
async bzpopmin(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bzpopmin', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'bzpopmin', args: [opts, ...args], resolve, reject })
    })
  }
}


async cluster(opts: ServerSelector, ...args: args): Promise<any>
async cluster(...args: args): Promise<any>
async cluster(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'cluster', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'cluster', args: [opts, ...args], resolve, reject })
    })
  }
}


async config(opts: ServerSelector, ...args: args): Promise<any>
async config(...args: args): Promise<any>
async config(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'config', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'config', args: [opts, ...args], resolve, reject })
    })
  }
}


async copy(opts: ServerSelector, ...args: args): Promise<any>
async copy(...args: args): Promise<any>
async copy(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'copy', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'copy', args: [opts, ...args], resolve, reject })
    })
  }
}


async dbsize(opts: ServerSelector, ...args: args): Promise<any>
async dbsize(...args: args): Promise<any>
async dbsize(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'dbsize', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'dbsize', args: [opts, ...args], resolve, reject })
    })
  }
}


async debug(opts: ServerSelector, ...args: args): Promise<any>
async debug(...args: args): Promise<any>
async debug(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'debug', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'debug', args: [opts, ...args], resolve, reject })
    })
  }
}


async decr(opts: ServerSelector, ...args: args): Promise<any>
async decr(...args: args): Promise<any>
async decr(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'decr', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'decr', args: [opts, ...args], resolve, reject })
    })
  }
}


async decrby(opts: ServerSelector, ...args: args): Promise<any>
async decrby(...args: args): Promise<any>
async decrby(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'decrby', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'decrby', args: [opts, ...args], resolve, reject })
    })
  }
}


async del(opts: ServerSelector, ...args: args): Promise<any>
async del(...args: args): Promise<any>
async del(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'del', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'del', args: [opts, ...args], resolve, reject })
    })
  }
}


async discard(opts: ServerSelector, ...args: args): Promise<any>
async discard(...args: args): Promise<any>
async discard(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'discard', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'discard', args: [opts, ...args], resolve, reject })
    })
  }
}


async dump(opts: ServerSelector, ...args: args): Promise<any>
async dump(...args: args): Promise<any>
async dump(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'dump', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'dump', args: [opts, ...args], resolve, reject })
    })
  }
}


async echo(opts: ServerSelector, ...args: args): Promise<any>
async echo(...args: args): Promise<any>
async echo(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'echo', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'echo', args: [opts, ...args], resolve, reject })
    })
  }
}


async eval(opts: ServerSelector, ...args: args): Promise<any>
async eval(...args: args): Promise<any>
async eval(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'eval', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'eval', args: [opts, ...args], resolve, reject })
    })
  }
}


async evalsha(opts: ServerSelector, ...args: args): Promise<any>
async evalsha(...args: args): Promise<any>
async evalsha(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'evalsha', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'evalsha', args: [opts, ...args], resolve, reject })
    })
  }
}


async exec(opts: ServerSelector, ...args: args): Promise<any>
async exec(...args: args): Promise<any>
async exec(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'exec', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'exec', args: [opts, ...args], resolve, reject })
    })
  }
}


async exists(opts: ServerSelector, ...args: args): Promise<any>
async exists(...args: args): Promise<any>
async exists(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'exists', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'exists', args: [opts, ...args], resolve, reject })
    })
  }
}


async expire(opts: ServerSelector, ...args: args): Promise<any>
async expire(...args: args): Promise<any>
async expire(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'expire', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'expire', args: [opts, ...args], resolve, reject })
    })
  }
}


async expireat(opts: ServerSelector, ...args: args): Promise<any>
async expireat(...args: args): Promise<any>
async expireat(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'expireat', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'expireat', args: [opts, ...args], resolve, reject })
    })
  }
}


async failover(opts: ServerSelector, ...args: args): Promise<any>
async failover(...args: args): Promise<any>
async failover(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'failover', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'failover', args: [opts, ...args], resolve, reject })
    })
  }
}


async flushall(opts: ServerSelector, ...args: args): Promise<any>
async flushall(...args: args): Promise<any>
async flushall(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'flushall', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'flushall', args: [opts, ...args], resolve, reject })
    })
  }
}


async flushdb(opts: ServerSelector, ...args: args): Promise<any>
async flushdb(...args: args): Promise<any>
async flushdb(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'flushdb', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'flushdb', args: [opts, ...args], resolve, reject })
    })
  }
}


async geoadd(opts: ServerSelector, ...args: args): Promise<any>
async geoadd(...args: args): Promise<any>
async geoadd(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geoadd', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geoadd', args: [opts, ...args], resolve, reject })
    })
  }
}


async geodist(opts: ServerSelector, ...args: args): Promise<any>
async geodist(...args: args): Promise<any>
async geodist(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geodist', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geodist', args: [opts, ...args], resolve, reject })
    })
  }
}


async geohash(opts: ServerSelector, ...args: args): Promise<any>
async geohash(...args: args): Promise<any>
async geohash(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geohash', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geohash', args: [opts, ...args], resolve, reject })
    })
  }
}


async geopos(opts: ServerSelector, ...args: args): Promise<any>
async geopos(...args: args): Promise<any>
async geopos(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geopos', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geopos', args: [opts, ...args], resolve, reject })
    })
  }
}


async georadius(opts: ServerSelector, ...args: args): Promise<any>
async georadius(...args: args): Promise<any>
async georadius(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'georadius', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'georadius', args: [opts, ...args], resolve, reject })
    })
  }
}


async georadius_ro(opts: ServerSelector, ...args: args): Promise<any>
async georadius_ro(...args: args): Promise<any>
async georadius_ro(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'georadius_ro', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'georadius_ro', args: [opts, ...args], resolve, reject })
    })
  }
}


async georadiusbymember(opts: ServerSelector, ...args: args): Promise<any>
async georadiusbymember(...args: args): Promise<any>
async georadiusbymember(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'georadiusbymember', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'georadiusbymember', args: [opts, ...args], resolve, reject })
    })
  }
}


async georadiusbymember_ro(opts: ServerSelector, ...args: args): Promise<any>
async georadiusbymember_ro(...args: args): Promise<any>
async georadiusbymember_ro(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'georadiusbymember_ro', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'georadiusbymember_ro', args: [opts, ...args], resolve, reject })
    })
  }
}


async geosearch(opts: ServerSelector, ...args: args): Promise<any>
async geosearch(...args: args): Promise<any>
async geosearch(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geosearch', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geosearch', args: [opts, ...args], resolve, reject })
    })
  }
}


async geosearchstore(opts: ServerSelector, ...args: args): Promise<any>
async geosearchstore(...args: args): Promise<any>
async geosearchstore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geosearchstore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'geosearchstore', args: [opts, ...args], resolve, reject })
    })
  }
}


async get(opts: ServerSelector, ...args: args): Promise<any>
async get(...args: args): Promise<any>
async get(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'get', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'get', args: [opts, ...args], resolve, reject })
    })
  }
}


async getbit(opts: ServerSelector, ...args: args): Promise<any>
async getbit(...args: args): Promise<any>
async getbit(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'getbit', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'getbit', args: [opts, ...args], resolve, reject })
    })
  }
}


async getdel(opts: ServerSelector, ...args: args): Promise<any>
async getdel(...args: args): Promise<any>
async getdel(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'getdel', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'getdel', args: [opts, ...args], resolve, reject })
    })
  }
}


async getex(opts: ServerSelector, ...args: args): Promise<any>
async getex(...args: args): Promise<any>
async getex(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'getex', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'getex', args: [opts, ...args], resolve, reject })
    })
  }
}


async getrange(opts: ServerSelector, ...args: args): Promise<any>
async getrange(...args: args): Promise<any>
async getrange(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'getrange', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'getrange', args: [opts, ...args], resolve, reject })
    })
  }
}


async getset(opts: ServerSelector, ...args: args): Promise<any>
async getset(...args: args): Promise<any>
async getset(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'getset', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'getset', args: [opts, ...args], resolve, reject })
    })
  }
}


async hdel(opts: ServerSelector, ...args: args): Promise<any>
async hdel(...args: args): Promise<any>
async hdel(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hdel', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hdel', args: [opts, ...args], resolve, reject })
    })
  }
}


async hello(opts: ServerSelector, ...args: args): Promise<any>
async hello(...args: args): Promise<any>
async hello(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hello', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hello', args: [opts, ...args], resolve, reject })
    })
  }
}


async hexists(opts: ServerSelector, ...args: args): Promise<any>
async hexists(...args: args): Promise<any>
async hexists(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hexists', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hexists', args: [opts, ...args], resolve, reject })
    })
  }
}


async hget(opts: ServerSelector, ...args: args): Promise<any>
async hget(...args: args): Promise<any>
async hget(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hget', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hget', args: [opts, ...args], resolve, reject })
    })
  }
}


async hgetall(opts: ServerSelector, ...args: args): Promise<any>
async hgetall(...args: args): Promise<any>
async hgetall(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hgetall', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hgetall', args: [opts, ...args], resolve, reject })
    })
  }
}


async hincrby(opts: ServerSelector, ...args: args): Promise<any>
async hincrby(...args: args): Promise<any>
async hincrby(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hincrby', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hincrby', args: [opts, ...args], resolve, reject })
    })
  }
}


async hincrbyfloat(opts: ServerSelector, ...args: args): Promise<any>
async hincrbyfloat(...args: args): Promise<any>
async hincrbyfloat(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hincrbyfloat', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hincrbyfloat', args: [opts, ...args], resolve, reject })
    })
  }
}


async hkeys(opts: ServerSelector, ...args: args): Promise<any>
async hkeys(...args: args): Promise<any>
async hkeys(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hkeys', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hkeys', args: [opts, ...args], resolve, reject })
    })
  }
}


async hlen(opts: ServerSelector, ...args: args): Promise<any>
async hlen(...args: args): Promise<any>
async hlen(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hlen', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hlen', args: [opts, ...args], resolve, reject })
    })
  }
}


async hmget(opts: ServerSelector, ...args: args): Promise<any>
async hmget(...args: args): Promise<any>
async hmget(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hmget', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hmget', args: [opts, ...args], resolve, reject })
    })
  }
}


async host_(opts: ServerSelector, ...args: args): Promise<any>
async host_(...args: args): Promise<any>
async host_(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'host_', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'host_', args: [opts, ...args], resolve, reject })
    })
  }
}


async hrandfield(opts: ServerSelector, ...args: args): Promise<any>
async hrandfield(...args: args): Promise<any>
async hrandfield(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hrandfield', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hrandfield', args: [opts, ...args], resolve, reject })
    })
  }
}


async hscan(opts: ServerSelector, ...args: args): Promise<any>
async hscan(...args: args): Promise<any>
async hscan(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hscan', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hscan', args: [opts, ...args], resolve, reject })
    })
  }
}


async hset(opts: ServerSelector, ...args: args): Promise<any>
async hset(...args: args): Promise<any>
async hset(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hset', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hset', args: [opts, ...args], resolve, reject })
    })
  }
}


async hsetnx(opts: ServerSelector, ...args: args): Promise<any>
async hsetnx(...args: args): Promise<any>
async hsetnx(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hsetnx', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hsetnx', args: [opts, ...args], resolve, reject })
    })
  }
}


async hstrlen(opts: ServerSelector, ...args: args): Promise<any>
async hstrlen(...args: args): Promise<any>
async hstrlen(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hstrlen', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hstrlen', args: [opts, ...args], resolve, reject })
    })
  }
}


async hvals(opts: ServerSelector, ...args: args): Promise<any>
async hvals(...args: args): Promise<any>
async hvals(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hvals', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'hvals', args: [opts, ...args], resolve, reject })
    })
  }
}


async incr(opts: ServerSelector, ...args: args): Promise<any>
async incr(...args: args): Promise<any>
async incr(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'incr', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'incr', args: [opts, ...args], resolve, reject })
    })
  }
}


async incrby(opts: ServerSelector, ...args: args): Promise<any>
async incrby(...args: args): Promise<any>
async incrby(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'incrby', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'incrby', args: [opts, ...args], resolve, reject })
    })
  }
}


async incrbyfloat(opts: ServerSelector, ...args: args): Promise<any>
async incrbyfloat(...args: args): Promise<any>
async incrbyfloat(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'incrbyfloat', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'incrbyfloat', args: [opts, ...args], resolve, reject })
    })
  }
}


async keys(opts: ServerSelector, ...args: args): Promise<any>
async keys(...args: args): Promise<any>
async keys(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'keys', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'keys', args: [opts, ...args], resolve, reject })
    })
  }
}


async lastsave(opts: ServerSelector, ...args: args): Promise<any>
async lastsave(...args: args): Promise<any>
async lastsave(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lastsave', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lastsave', args: [opts, ...args], resolve, reject })
    })
  }
}


async latency(opts: ServerSelector, ...args: args): Promise<any>
async latency(...args: args): Promise<any>
async latency(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'latency', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'latency', args: [opts, ...args], resolve, reject })
    })
  }
}


async lindex(opts: ServerSelector, ...args: args): Promise<any>
async lindex(...args: args): Promise<any>
async lindex(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lindex', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lindex', args: [opts, ...args], resolve, reject })
    })
  }
}


async linsert(opts: ServerSelector, ...args: args): Promise<any>
async linsert(...args: args): Promise<any>
async linsert(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'linsert', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'linsert', args: [opts, ...args], resolve, reject })
    })
  }
}


async llen(opts: ServerSelector, ...args: args): Promise<any>
async llen(...args: args): Promise<any>
async llen(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'llen', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'llen', args: [opts, ...args], resolve, reject })
    })
  }
}


async lmove(opts: ServerSelector, ...args: args): Promise<any>
async lmove(...args: args): Promise<any>
async lmove(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lmove', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lmove', args: [opts, ...args], resolve, reject })
    })
  }
}


async lolwut(opts: ServerSelector, ...args: args): Promise<any>
async lolwut(...args: args): Promise<any>
async lolwut(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lolwut', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lolwut', args: [opts, ...args], resolve, reject })
    })
  }
}


async lpop(opts: ServerSelector, ...args: args): Promise<any>
async lpop(...args: args): Promise<any>
async lpop(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lpop', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lpop', args: [opts, ...args], resolve, reject })
    })
  }
}


async lpos(opts: ServerSelector, ...args: args): Promise<any>
async lpos(...args: args): Promise<any>
async lpos(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lpos', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lpos', args: [opts, ...args], resolve, reject })
    })
  }
}


async lpush(opts: ServerSelector, ...args: args): Promise<any>
async lpush(...args: args): Promise<any>
async lpush(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lpush', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lpush', args: [opts, ...args], resolve, reject })
    })
  }
}


async lpushx(opts: ServerSelector, ...args: args): Promise<any>
async lpushx(...args: args): Promise<any>
async lpushx(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lpushx', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lpushx', args: [opts, ...args], resolve, reject })
    })
  }
}


async lrange(opts: ServerSelector, ...args: args): Promise<any>
async lrange(...args: args): Promise<any>
async lrange(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lrange', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lrange', args: [opts, ...args], resolve, reject })
    })
  }
}


async lrem(opts: ServerSelector, ...args: args): Promise<any>
async lrem(...args: args): Promise<any>
async lrem(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lrem', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lrem', args: [opts, ...args], resolve, reject })
    })
  }
}


async lset(opts: ServerSelector, ...args: args): Promise<any>
async lset(...args: args): Promise<any>
async lset(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lset', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'lset', args: [opts, ...args], resolve, reject })
    })
  }
}


async ltrim(opts: ServerSelector, ...args: args): Promise<any>
async ltrim(...args: args): Promise<any>
async ltrim(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'ltrim', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'ltrim', args: [opts, ...args], resolve, reject })
    })
  }
}


async memory(opts: ServerSelector, ...args: args): Promise<any>
async memory(...args: args): Promise<any>
async memory(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'memory', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'memory', args: [opts, ...args], resolve, reject })
    })
  }
}


async mget(opts: ServerSelector, ...args: args): Promise<any>
async mget(...args: args): Promise<any>
async mget(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'mget', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'mget', args: [opts, ...args], resolve, reject })
    })
  }
}


async migrate(opts: ServerSelector, ...args: args): Promise<any>
async migrate(...args: args): Promise<any>
async migrate(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'migrate', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'migrate', args: [opts, ...args], resolve, reject })
    })
  }
}


async module(opts: ServerSelector, ...args: args): Promise<any>
async module(...args: args): Promise<any>
async module(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'module', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'module', args: [opts, ...args], resolve, reject })
    })
  }
}


async move(opts: ServerSelector, ...args: args): Promise<any>
async move(...args: args): Promise<any>
async move(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'move', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'move', args: [opts, ...args], resolve, reject })
    })
  }
}


async mset(opts: ServerSelector, ...args: args): Promise<any>
async mset(...args: args): Promise<any>
async mset(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'mset', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'mset', args: [opts, ...args], resolve, reject })
    })
  }
}


async msetnx(opts: ServerSelector, ...args: args): Promise<any>
async msetnx(...args: args): Promise<any>
async msetnx(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'msetnx', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'msetnx', args: [opts, ...args], resolve, reject })
    })
  }
}


async object(opts: ServerSelector, ...args: args): Promise<any>
async object(...args: args): Promise<any>
async object(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'object', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'object', args: [opts, ...args], resolve, reject })
    })
  }
}


async persist(opts: ServerSelector, ...args: args): Promise<any>
async persist(...args: args): Promise<any>
async persist(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'persist', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'persist', args: [opts, ...args], resolve, reject })
    })
  }
}


async pexpire(opts: ServerSelector, ...args: args): Promise<any>
async pexpire(...args: args): Promise<any>
async pexpire(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pexpire', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pexpire', args: [opts, ...args], resolve, reject })
    })
  }
}


async pexpireat(opts: ServerSelector, ...args: args): Promise<any>
async pexpireat(...args: args): Promise<any>
async pexpireat(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pexpireat', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pexpireat', args: [opts, ...args], resolve, reject })
    })
  }
}


async pfadd(opts: ServerSelector, ...args: args): Promise<any>
async pfadd(...args: args): Promise<any>
async pfadd(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pfadd', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pfadd', args: [opts, ...args], resolve, reject })
    })
  }
}


async pfcount(opts: ServerSelector, ...args: args): Promise<any>
async pfcount(...args: args): Promise<any>
async pfcount(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pfcount', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pfcount', args: [opts, ...args], resolve, reject })
    })
  }
}


async pfdebug(opts: ServerSelector, ...args: args): Promise<any>
async pfdebug(...args: args): Promise<any>
async pfdebug(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pfdebug', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pfdebug', args: [opts, ...args], resolve, reject })
    })
  }
}


async pfmerge(opts: ServerSelector, ...args: args): Promise<any>
async pfmerge(...args: args): Promise<any>
async pfmerge(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pfmerge', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pfmerge', args: [opts, ...args], resolve, reject })
    })
  }
}


async pfselftest(opts: ServerSelector, ...args: args): Promise<any>
async pfselftest(...args: args): Promise<any>
async pfselftest(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pfselftest', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pfselftest', args: [opts, ...args], resolve, reject })
    })
  }
}


async ping(opts: ServerSelector, ...args: args): Promise<any>
async ping(...args: args): Promise<any>
async ping(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'ping', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'ping', args: [opts, ...args], resolve, reject })
    })
  }
}


async post(opts: ServerSelector, ...args: args): Promise<any>
async post(...args: args): Promise<any>
async post(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'post', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'post', args: [opts, ...args], resolve, reject })
    })
  }
}


async psetex(opts: ServerSelector, ...args: args): Promise<any>
async psetex(...args: args): Promise<any>
async psetex(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'psetex', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'psetex', args: [opts, ...args], resolve, reject })
    })
  }
}


async psync(opts: ServerSelector, ...args: args): Promise<any>
async psync(...args: args): Promise<any>
async psync(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'psync', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'psync', args: [opts, ...args], resolve, reject })
    })
  }
}


async pttl(opts: ServerSelector, ...args: args): Promise<any>
async pttl(...args: args): Promise<any>
async pttl(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pttl', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pttl', args: [opts, ...args], resolve, reject })
    })
  }
}


async publish(opts: ServerSelector, ...args: args): Promise<any>
async publish(...args: args): Promise<any>
async publish(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'publish', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'publish', args: [opts, ...args], resolve, reject })
    })
  }
}


async pubsub(opts: ServerSelector, ...args: args): Promise<any>
async pubsub(...args: args): Promise<any>
async pubsub(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pubsub', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'pubsub', args: [opts, ...args], resolve, reject })
    })
  }
}


async randomkey(opts: ServerSelector, ...args: args): Promise<any>
async randomkey(...args: args): Promise<any>
async randomkey(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'randomkey', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'randomkey', args: [opts, ...args], resolve, reject })
    })
  }
}


async readonly(opts: ServerSelector, ...args: args): Promise<any>
async readonly(...args: args): Promise<any>
async readonly(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'readonly', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'readonly', args: [opts, ...args], resolve, reject })
    })
  }
}


async readwrite(opts: ServerSelector, ...args: args): Promise<any>
async readwrite(...args: args): Promise<any>
async readwrite(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'readwrite', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'readwrite', args: [opts, ...args], resolve, reject })
    })
  }
}


async rename(opts: ServerSelector, ...args: args): Promise<any>
async rename(...args: args): Promise<any>
async rename(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'rename', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'rename', args: [opts, ...args], resolve, reject })
    })
  }
}


async renamenx(opts: ServerSelector, ...args: args): Promise<any>
async renamenx(...args: args): Promise<any>
async renamenx(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'renamenx', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'renamenx', args: [opts, ...args], resolve, reject })
    })
  }
}


async replconf(opts: ServerSelector, ...args: args): Promise<any>
async replconf(...args: args): Promise<any>
async replconf(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'replconf', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'replconf', args: [opts, ...args], resolve, reject })
    })
  }
}


async replicaof(opts: ServerSelector, ...args: args): Promise<any>
async replicaof(...args: args): Promise<any>
async replicaof(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'replicaof', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'replicaof', args: [opts, ...args], resolve, reject })
    })
  }
}


async reset(opts: ServerSelector, ...args: args): Promise<any>
async reset(...args: args): Promise<any>
async reset(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'reset', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'reset', args: [opts, ...args], resolve, reject })
    })
  }
}


async restore(opts: ServerSelector, ...args: args): Promise<any>
async restore(...args: args): Promise<any>
async restore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'restore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'restore', args: [opts, ...args], resolve, reject })
    })
  }
}


async restore_asking(opts: ServerSelector, ...args: args): Promise<any>
async restore_asking(...args: args): Promise<any>
async restore_asking(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'restore_asking', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'restore_asking', args: [opts, ...args], resolve, reject })
    })
  }
}


async role(opts: ServerSelector, ...args: args): Promise<any>
async role(...args: args): Promise<any>
async role(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'role', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'role', args: [opts, ...args], resolve, reject })
    })
  }
}


async rpop(opts: ServerSelector, ...args: args): Promise<any>
async rpop(...args: args): Promise<any>
async rpop(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'rpop', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'rpop', args: [opts, ...args], resolve, reject })
    })
  }
}


async rpoplpush(opts: ServerSelector, ...args: args): Promise<any>
async rpoplpush(...args: args): Promise<any>
async rpoplpush(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'rpoplpush', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'rpoplpush', args: [opts, ...args], resolve, reject })
    })
  }
}


async rpush(opts: ServerSelector, ...args: args): Promise<any>
async rpush(...args: args): Promise<any>
async rpush(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'rpush', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'rpush', args: [opts, ...args], resolve, reject })
    })
  }
}


async rpushx(opts: ServerSelector, ...args: args): Promise<any>
async rpushx(...args: args): Promise<any>
async rpushx(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'rpushx', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'rpushx', args: [opts, ...args], resolve, reject })
    })
  }
}


async sadd(opts: ServerSelector, ...args: args): Promise<any>
async sadd(...args: args): Promise<any>
async sadd(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sadd', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sadd', args: [opts, ...args], resolve, reject })
    })
  }
}


async save(opts: ServerSelector, ...args: args): Promise<any>
async save(...args: args): Promise<any>
async save(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'save', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'save', args: [opts, ...args], resolve, reject })
    })
  }
}


async scan(opts: ServerSelector, ...args: args): Promise<any>
async scan(...args: args): Promise<any>
async scan(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'scan', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'scan', args: [opts, ...args], resolve, reject })
    })
  }
}


async scard(opts: ServerSelector, ...args: args): Promise<any>
async scard(...args: args): Promise<any>
async scard(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'scard', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'scard', args: [opts, ...args], resolve, reject })
    })
  }
}


async script(opts: ServerSelector, ...args: args): Promise<any>
async script(...args: args): Promise<any>
async script(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'script', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'script', args: [opts, ...args], resolve, reject })
    })
  }
}


async sdiff(opts: ServerSelector, ...args: args): Promise<any>
async sdiff(...args: args): Promise<any>
async sdiff(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sdiff', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sdiff', args: [opts, ...args], resolve, reject })
    })
  }
}


async sdiffstore(opts: ServerSelector, ...args: args): Promise<any>
async sdiffstore(...args: args): Promise<any>
async sdiffstore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sdiffstore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sdiffstore', args: [opts, ...args], resolve, reject })
    })
  }
}


async set(opts: ServerSelector, ...args: args): Promise<any>
async set(...args: args): Promise<any>
async set(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'set', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'set', args: [opts, ...args], resolve, reject })
    })
  }
}


async setbit(opts: ServerSelector, ...args: args): Promise<any>
async setbit(...args: args): Promise<any>
async setbit(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'setbit', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'setbit', args: [opts, ...args], resolve, reject })
    })
  }
}


async setex(opts: ServerSelector, ...args: args): Promise<any>
async setex(...args: args): Promise<any>
async setex(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'setex', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'setex', args: [opts, ...args], resolve, reject })
    })
  }
}


async setnx(opts: ServerSelector, ...args: args): Promise<any>
async setnx(...args: args): Promise<any>
async setnx(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'setnx', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'setnx', args: [opts, ...args], resolve, reject })
    })
  }
}


async setrange(opts: ServerSelector, ...args: args): Promise<any>
async setrange(...args: args): Promise<any>
async setrange(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'setrange', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'setrange', args: [opts, ...args], resolve, reject })
    })
  }
}


async shutdown(opts: ServerSelector, ...args: args): Promise<any>
async shutdown(...args: args): Promise<any>
async shutdown(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'shutdown', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'shutdown', args: [opts, ...args], resolve, reject })
    })
  }
}


async sinter(opts: ServerSelector, ...args: args): Promise<any>
async sinter(...args: args): Promise<any>
async sinter(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sinter', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sinter', args: [opts, ...args], resolve, reject })
    })
  }
}


async sinterstore(opts: ServerSelector, ...args: args): Promise<any>
async sinterstore(...args: args): Promise<any>
async sinterstore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sinterstore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sinterstore', args: [opts, ...args], resolve, reject })
    })
  }
}


async sismember(opts: ServerSelector, ...args: args): Promise<any>
async sismember(...args: args): Promise<any>
async sismember(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sismember', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sismember', args: [opts, ...args], resolve, reject })
    })
  }
}


async slaveof(opts: ServerSelector, ...args: args): Promise<any>
async slaveof(...args: args): Promise<any>
async slaveof(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'slaveof', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'slaveof', args: [opts, ...args], resolve, reject })
    })
  }
}


async slowlog(opts: ServerSelector, ...args: args): Promise<any>
async slowlog(...args: args): Promise<any>
async slowlog(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'slowlog', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'slowlog', args: [opts, ...args], resolve, reject })
    })
  }
}


async smembers(opts: ServerSelector, ...args: args): Promise<any>
async smembers(...args: args): Promise<any>
async smembers(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'smembers', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'smembers', args: [opts, ...args], resolve, reject })
    })
  }
}


async smismember(opts: ServerSelector, ...args: args): Promise<any>
async smismember(...args: args): Promise<any>
async smismember(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'smismember', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'smismember', args: [opts, ...args], resolve, reject })
    })
  }
}


async smove(opts: ServerSelector, ...args: args): Promise<any>
async smove(...args: args): Promise<any>
async smove(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'smove', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'smove', args: [opts, ...args], resolve, reject })
    })
  }
}


async sort(opts: ServerSelector, ...args: args): Promise<any>
async sort(...args: args): Promise<any>
async sort(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sort', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sort', args: [opts, ...args], resolve, reject })
    })
  }
}


async spop(opts: ServerSelector, ...args: args): Promise<any>
async spop(...args: args): Promise<any>
async spop(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'spop', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'spop', args: [opts, ...args], resolve, reject })
    })
  }
}


async srandmember(opts: ServerSelector, ...args: args): Promise<any>
async srandmember(...args: args): Promise<any>
async srandmember(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'srandmember', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'srandmember', args: [opts, ...args], resolve, reject })
    })
  }
}


async srem(opts: ServerSelector, ...args: args): Promise<any>
async srem(...args: args): Promise<any>
async srem(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'srem', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'srem', args: [opts, ...args], resolve, reject })
    })
  }
}


async sscan(opts: ServerSelector, ...args: args): Promise<any>
async sscan(...args: args): Promise<any>
async sscan(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sscan', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sscan', args: [opts, ...args], resolve, reject })
    })
  }
}


async stralgo(opts: ServerSelector, ...args: args): Promise<any>
async stralgo(...args: args): Promise<any>
async stralgo(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'stralgo', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'stralgo', args: [opts, ...args], resolve, reject })
    })
  }
}


async strlen(opts: ServerSelector, ...args: args): Promise<any>
async strlen(...args: args): Promise<any>
async strlen(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'strlen', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'strlen', args: [opts, ...args], resolve, reject })
    })
  }
}


async substr(opts: ServerSelector, ...args: args): Promise<any>
async substr(...args: args): Promise<any>
async substr(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'substr', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'substr', args: [opts, ...args], resolve, reject })
    })
  }
}


async sunion(opts: ServerSelector, ...args: args): Promise<any>
async sunion(...args: args): Promise<any>
async sunion(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sunion', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sunion', args: [opts, ...args], resolve, reject })
    })
  }
}


async sunionstore(opts: ServerSelector, ...args: args): Promise<any>
async sunionstore(...args: args): Promise<any>
async sunionstore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sunionstore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sunionstore', args: [opts, ...args], resolve, reject })
    })
  }
}


async swapdb(opts: ServerSelector, ...args: args): Promise<any>
async swapdb(...args: args): Promise<any>
async swapdb(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'swapdb', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'swapdb', args: [opts, ...args], resolve, reject })
    })
  }
}


async sync(opts: ServerSelector, ...args: args): Promise<any>
async sync(...args: args): Promise<any>
async sync(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sync', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'sync', args: [opts, ...args], resolve, reject })
    })
  }
}


async time(opts: ServerSelector, ...args: args): Promise<any>
async time(...args: args): Promise<any>
async time(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'time', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'time', args: [opts, ...args], resolve, reject })
    })
  }
}


async touch(opts: ServerSelector, ...args: args): Promise<any>
async touch(...args: args): Promise<any>
async touch(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'touch', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'touch', args: [opts, ...args], resolve, reject })
    })
  }
}


async ttl(opts: ServerSelector, ...args: args): Promise<any>
async ttl(...args: args): Promise<any>
async ttl(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'ttl', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'ttl', args: [opts, ...args], resolve, reject })
    })
  }
}


async type(opts: ServerSelector, ...args: args): Promise<any>
async type(...args: args): Promise<any>
async type(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'type', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'type', args: [opts, ...args], resolve, reject })
    })
  }
}


async unlink(opts: ServerSelector, ...args: args): Promise<any>
async unlink(...args: args): Promise<any>
async unlink(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'unlink', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'unlink', args: [opts, ...args], resolve, reject })
    })
  }
}


async unwatch(opts: ServerSelector, ...args: args): Promise<any>
async unwatch(...args: args): Promise<any>
async unwatch(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'unwatch', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'unwatch', args: [opts, ...args], resolve, reject })
    })
  }
}


async wait(opts: ServerSelector, ...args: args): Promise<any>
async wait(...args: args): Promise<any>
async wait(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'wait', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'wait', args: [opts, ...args], resolve, reject })
    })
  }
}


async watch(opts: ServerSelector, ...args: args): Promise<any>
async watch(...args: args): Promise<any>
async watch(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'watch', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'watch', args: [opts, ...args], resolve, reject })
    })
  }
}


async xack(opts: ServerSelector, ...args: args): Promise<any>
async xack(...args: args): Promise<any>
async xack(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xack', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xack', args: [opts, ...args], resolve, reject })
    })
  }
}


async xadd(opts: ServerSelector, ...args: args): Promise<any>
async xadd(...args: args): Promise<any>
async xadd(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xadd', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xadd', args: [opts, ...args], resolve, reject })
    })
  }
}


async xautoclaim(opts: ServerSelector, ...args: args): Promise<any>
async xautoclaim(...args: args): Promise<any>
async xautoclaim(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xautoclaim', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xautoclaim', args: [opts, ...args], resolve, reject })
    })
  }
}


async xclaim(opts: ServerSelector, ...args: args): Promise<any>
async xclaim(...args: args): Promise<any>
async xclaim(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xclaim', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xclaim', args: [opts, ...args], resolve, reject })
    })
  }
}


async xdel(opts: ServerSelector, ...args: args): Promise<any>
async xdel(...args: args): Promise<any>
async xdel(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xdel', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xdel', args: [opts, ...args], resolve, reject })
    })
  }
}


async xgroup(opts: ServerSelector, ...args: args): Promise<any>
async xgroup(...args: args): Promise<any>
async xgroup(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xgroup', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xgroup', args: [opts, ...args], resolve, reject })
    })
  }
}


async xinfo(opts: ServerSelector, ...args: args): Promise<any>
async xinfo(...args: args): Promise<any>
async xinfo(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xinfo', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xinfo', args: [opts, ...args], resolve, reject })
    })
  }
}


async xlen(opts: ServerSelector, ...args: args): Promise<any>
async xlen(...args: args): Promise<any>
async xlen(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xlen', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xlen', args: [opts, ...args], resolve, reject })
    })
  }
}


async xpending(opts: ServerSelector, ...args: args): Promise<any>
async xpending(...args: args): Promise<any>
async xpending(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xpending', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xpending', args: [opts, ...args], resolve, reject })
    })
  }
}


async xrange(opts: ServerSelector, ...args: args): Promise<any>
async xrange(...args: args): Promise<any>
async xrange(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xrange', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xrange', args: [opts, ...args], resolve, reject })
    })
  }
}


async xread(opts: ServerSelector, ...args: args): Promise<any>
async xread(...args: args): Promise<any>
async xread(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xread', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xread', args: [opts, ...args], resolve, reject })
    })
  }
}


async xreadgroup(opts: ServerSelector, ...args: args): Promise<any>
async xreadgroup(...args: args): Promise<any>
async xreadgroup(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xreadgroup', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xreadgroup', args: [opts, ...args], resolve, reject })
    })
  }
}


async xrevrange(opts: ServerSelector, ...args: args): Promise<any>
async xrevrange(...args: args): Promise<any>
async xrevrange(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xrevrange', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xrevrange', args: [opts, ...args], resolve, reject })
    })
  }
}


async xsetid(opts: ServerSelector, ...args: args): Promise<any>
async xsetid(...args: args): Promise<any>
async xsetid(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xsetid', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xsetid', args: [opts, ...args], resolve, reject })
    })
  }
}


async xtrim(opts: ServerSelector, ...args: args): Promise<any>
async xtrim(...args: args): Promise<any>
async xtrim(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xtrim', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'xtrim', args: [opts, ...args], resolve, reject })
    })
  }
}


async zadd(opts: ServerSelector, ...args: args): Promise<any>
async zadd(...args: args): Promise<any>
async zadd(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zadd', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zadd', args: [opts, ...args], resolve, reject })
    })
  }
}


async zcard(opts: ServerSelector, ...args: args): Promise<any>
async zcard(...args: args): Promise<any>
async zcard(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zcard', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zcard', args: [opts, ...args], resolve, reject })
    })
  }
}


async zcount(opts: ServerSelector, ...args: args): Promise<any>
async zcount(...args: args): Promise<any>
async zcount(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zcount', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zcount', args: [opts, ...args], resolve, reject })
    })
  }
}


async zdiff(opts: ServerSelector, ...args: args): Promise<any>
async zdiff(...args: args): Promise<any>
async zdiff(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zdiff', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zdiff', args: [opts, ...args], resolve, reject })
    })
  }
}


async zdiffstore(opts: ServerSelector, ...args: args): Promise<any>
async zdiffstore(...args: args): Promise<any>
async zdiffstore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zdiffstore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zdiffstore', args: [opts, ...args], resolve, reject })
    })
  }
}


async zincrby(opts: ServerSelector, ...args: args): Promise<any>
async zincrby(...args: args): Promise<any>
async zincrby(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zincrby', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zincrby', args: [opts, ...args], resolve, reject })
    })
  }
}


async zinter(opts: ServerSelector, ...args: args): Promise<any>
async zinter(...args: args): Promise<any>
async zinter(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zinter', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zinter', args: [opts, ...args], resolve, reject })
    })
  }
}


async zinterstore(opts: ServerSelector, ...args: args): Promise<any>
async zinterstore(...args: args): Promise<any>
async zinterstore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zinterstore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zinterstore', args: [opts, ...args], resolve, reject })
    })
  }
}


async zlexcount(opts: ServerSelector, ...args: args): Promise<any>
async zlexcount(...args: args): Promise<any>
async zlexcount(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zlexcount', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zlexcount', args: [opts, ...args], resolve, reject })
    })
  }
}


async zmscore(opts: ServerSelector, ...args: args): Promise<any>
async zmscore(...args: args): Promise<any>
async zmscore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zmscore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zmscore', args: [opts, ...args], resolve, reject })
    })
  }
}


async zpopmax(opts: ServerSelector, ...args: args): Promise<any>
async zpopmax(...args: args): Promise<any>
async zpopmax(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zpopmax', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zpopmax', args: [opts, ...args], resolve, reject })
    })
  }
}


async zpopmin(opts: ServerSelector, ...args: args): Promise<any>
async zpopmin(...args: args): Promise<any>
async zpopmin(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zpopmin', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zpopmin', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrandmember(opts: ServerSelector, ...args: args): Promise<any>
async zrandmember(...args: args): Promise<any>
async zrandmember(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrandmember', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrandmember', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrange(opts: ServerSelector, ...args: args): Promise<any>
async zrange(...args: args): Promise<any>
async zrange(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrange', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrange', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrangebylex(opts: ServerSelector, ...args: args): Promise<any>
async zrangebylex(...args: args): Promise<any>
async zrangebylex(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrangebylex', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrangebylex', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrangebyscore(opts: ServerSelector, ...args: args): Promise<any>
async zrangebyscore(...args: args): Promise<any>
async zrangebyscore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrangebyscore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrangebyscore', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrangestore(opts: ServerSelector, ...args: args): Promise<any>
async zrangestore(...args: args): Promise<any>
async zrangestore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrangestore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrangestore', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrank(opts: ServerSelector, ...args: args): Promise<any>
async zrank(...args: args): Promise<any>
async zrank(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrank', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrank', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrem(opts: ServerSelector, ...args: args): Promise<any>
async zrem(...args: args): Promise<any>
async zrem(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrem', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrem', args: [opts, ...args], resolve, reject })
    })
  }
}


async zremrangebylex(opts: ServerSelector, ...args: args): Promise<any>
async zremrangebylex(...args: args): Promise<any>
async zremrangebylex(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zremrangebylex', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zremrangebylex', args: [opts, ...args], resolve, reject })
    })
  }
}


async zremrangebyrank(opts: ServerSelector, ...args: args): Promise<any>
async zremrangebyrank(...args: args): Promise<any>
async zremrangebyrank(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zremrangebyrank', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zremrangebyrank', args: [opts, ...args], resolve, reject })
    })
  }
}


async zremrangebyscore(opts: ServerSelector, ...args: args): Promise<any>
async zremrangebyscore(...args: args): Promise<any>
async zremrangebyscore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zremrangebyscore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zremrangebyscore', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrevrange(opts: ServerSelector, ...args: args): Promise<any>
async zrevrange(...args: args): Promise<any>
async zrevrange(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrevrange', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrevrange', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrevrangebylex(opts: ServerSelector, ...args: args): Promise<any>
async zrevrangebylex(...args: args): Promise<any>
async zrevrangebylex(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrevrangebylex', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrevrangebylex', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrevrangebyscore(opts: ServerSelector, ...args: args): Promise<any>
async zrevrangebyscore(...args: args): Promise<any>
async zrevrangebyscore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrevrangebyscore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrevrangebyscore', args: [opts, ...args], resolve, reject })
    })
  }
}


async zrevrank(opts: ServerSelector, ...args: args): Promise<any>
async zrevrank(...args: args): Promise<any>
async zrevrank(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrevrank', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zrevrank', args: [opts, ...args], resolve, reject })
    })
  }
}


async zscan(opts: ServerSelector, ...args: args): Promise<any>
async zscan(...args: args): Promise<any>
async zscan(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zscan', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zscan', args: [opts, ...args], resolve, reject })
    })
  }
}


async zscore(opts: ServerSelector, ...args: args): Promise<any>
async zscore(...args: args): Promise<any>
async zscore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zscore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zscore', args: [opts, ...args], resolve, reject })
    })
  }
}


async zunion(opts: ServerSelector, ...args: args): Promise<any>
async zunion(...args: args): Promise<any>
async zunion(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zunion', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zunion', args: [opts, ...args], resolve, reject })
    })
  }
}


async zunionstore(opts: ServerSelector, ...args: args): Promise<any>
async zunionstore(...args: args): Promise<any>
async zunionstore(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zunionstore', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'zunionstore', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_hierarchy_children(opts: ServerSelector, ...args: args): Promise<any>
async selva_hierarchy_children(...args: args): Promise<any>
async selva_hierarchy_children(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_children', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_children', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_hierarchy_del(opts: ServerSelector, ...args: args): Promise<any>
async selva_hierarchy_del(...args: args): Promise<any>
async selva_hierarchy_del(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_del', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_del', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_hierarchy_find(opts: ServerSelector, ...args: args): Promise<any>
async selva_hierarchy_find(...args: args): Promise<any>
async selva_hierarchy_find(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_find', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_find', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_hierarchy_findin(opts: ServerSelector, ...args: args): Promise<any>
async selva_hierarchy_findin(...args: args): Promise<any>
async selva_hierarchy_findin(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_findin', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_findin', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_hierarchy_findrecursive(opts: ServerSelector, ...args: args): Promise<any>
async selva_hierarchy_findrecursive(...args: args): Promise<any>
async selva_hierarchy_findrecursive(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_findrecursive', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_findrecursive', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_hierarchy_findinsub(opts: ServerSelector, ...args: args): Promise<any>
async selva_hierarchy_findinsub(...args: args): Promise<any>
async selva_hierarchy_findinsub(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_findinsub', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_findinsub', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_hierarchy_parents(opts: ServerSelector, ...args: args): Promise<any>
async selva_hierarchy_parents(...args: args): Promise<any>
async selva_hierarchy_parents(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_parents', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_parents', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_hierarchy_edgelist(opts: ServerSelector, ...args: args): Promise<any>
async selva_hierarchy_edgelist(...args: args): Promise<any>
async selva_hierarchy_edgelist(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_edgelist', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_edgelist', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_hierarchy_edgeget(opts: ServerSelector, ...args: args): Promise<any>
async selva_hierarchy_edgeget(...args: args): Promise<any>
async selva_hierarchy_edgeget(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_edgeget', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_hierarchy_edgeget', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_inherit(opts: ServerSelector, ...args: args): Promise<any>
async selva_inherit(...args: args): Promise<any>
async selva_inherit(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_inherit', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_inherit', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_modify(opts: ServerSelector, ...args: args): Promise<any>
async selva_modify(...args: args): Promise<any>
async selva_modify(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_modify', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_modify', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_resolve_nodeid(opts: ServerSelector, ...args: args): Promise<any>
async selva_resolve_nodeid(...args: args): Promise<any>
async selva_resolve_nodeid(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_resolve_nodeid', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_resolve_nodeid', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_object_del(opts: ServerSelector, ...args: args): Promise<any>
async selva_object_del(...args: args): Promise<any>
async selva_object_del(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_del', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_del', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_object_exists(opts: ServerSelector, ...args: args): Promise<any>
async selva_object_exists(...args: args): Promise<any>
async selva_object_exists(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_exists', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_exists', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_object_get(opts: ServerSelector, ...args: args): Promise<any>
async selva_object_get(...args: args): Promise<any>
async selva_object_get(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_get', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_get', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_object_len(opts: ServerSelector, ...args: args): Promise<any>
async selva_object_len(...args: args): Promise<any>
async selva_object_len(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_len', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_len', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_object_set(opts: ServerSelector, ...args: args): Promise<any>
async selva_object_set(...args: args): Promise<any>
async selva_object_set(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_set', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_set', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_object_type(opts: ServerSelector, ...args: args): Promise<any>
async selva_object_type(...args: args): Promise<any>
async selva_object_type(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_type', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_type', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_object_getmeta(opts: ServerSelector, ...args: args): Promise<any>
async selva_object_getmeta(...args: args): Promise<any>
async selva_object_getmeta(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_getmeta', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_getmeta', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_object_setmeta(opts: ServerSelector, ...args: args): Promise<any>
async selva_object_setmeta(...args: args): Promise<any>
async selva_object_setmeta(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_setmeta', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_object_setmeta', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_subscriptions_add(opts: ServerSelector, ...args: args): Promise<any>
async selva_subscriptions_add(...args: args): Promise<any>
async selva_subscriptions_add(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_add', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_add', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_subscriptions_addmarkerfields(opts: ServerSelector, ...args: args): Promise<any>
async selva_subscriptions_addmarkerfields(...args: args): Promise<any>
async selva_subscriptions_addmarkerfields(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_addmarkerfields', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_addmarkerfields', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_subscriptions_addalias(opts: ServerSelector, ...args: args): Promise<any>
async selva_subscriptions_addalias(...args: args): Promise<any>
async selva_subscriptions_addalias(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_addalias', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_addalias', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_subscriptions_addmissing(opts: ServerSelector, ...args: args): Promise<any>
async selva_subscriptions_addmissing(...args: args): Promise<any>
async selva_subscriptions_addmissing(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_addmissing', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_addmissing', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_subscriptions_addtrigger(opts: ServerSelector, ...args: args): Promise<any>
async selva_subscriptions_addtrigger(...args: args): Promise<any>
async selva_subscriptions_addtrigger(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_addtrigger', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_addtrigger', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_subscriptions_debug(opts: ServerSelector, ...args: args): Promise<any>
async selva_subscriptions_debug(...args: args): Promise<any>
async selva_subscriptions_debug(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_debug', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_debug', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_subscriptions_del(opts: ServerSelector, ...args: args): Promise<any>
async selva_subscriptions_del(...args: args): Promise<any>
async selva_subscriptions_del(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_del', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_del', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_subscriptions_list(opts: ServerSelector, ...args: args): Promise<any>
async selva_subscriptions_list(...args: args): Promise<any>
async selva_subscriptions_list(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_list', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_list', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_subscriptions_listmissing(opts: ServerSelector, ...args: args): Promise<any>
async selva_subscriptions_listmissing(...args: args): Promise<any>
async selva_subscriptions_listmissing(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_listmissing', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_listmissing', args: [opts, ...args], resolve, reject })
    })
  }
}


async selva_subscriptions_refresh(opts: ServerSelector, ...args: args): Promise<any>
async selva_subscriptions_refresh(...args: args): Promise<any>
async selva_subscriptions_refresh(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_refresh', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: 'selva_subscriptions_refresh', args: [opts, ...args], resolve, reject })
    })
  }
}

}

export default RedisMethods
