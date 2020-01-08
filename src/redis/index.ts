import { createClient, RedisClient as Redis } from 'redis'
import fnv1a from '@sindresorhus/fnv1a'
import RedisMethods from './methods'

export type ConnectOptions = {
  port: number
  host?: string
  retryStrategy?: () => number // make a good default dont want to confiure this all the time
  // how we currently use this type is a 'service type' which can hold a bit more info, like name, id etc of the service)
}

type Resolvable = {
  resolve: (x: any) => void
  reject: (x: Error) => void
}

type RedisCommand = Resolvable & {
  command: string
  args: (string | number)[]
  hash?: number
  nested?: Resolvable[]
}

export default class RedisClient extends RedisMethods {
  private connector: () => Promise<ConnectOptions>
  private client: Redis
  private buffer: RedisCommand[]
  private connected: boolean
  private inProgress: boolean
  // private bufferedGet: Record<number, RedisCommand>

  constructor(connect: ConnectOptions | (() => Promise<ConnectOptions>)) {
    super()
    this.connector =
      typeof connect === 'object' ? () => Promise.resolve(connect) : connect
    this.buffer = []
    this.connect()
  }

  private async connect() {
    const opts = await this.connector()

    // even if the db does not exists should not crash!
    if (!opts.retryStrategy) {
      opts.retryStrategy = () => {
        return 1e3
      }
    }

    // on dc needs to re run connector - if different reconnect
    this.client = createClient(opts)

    this.client.on('error', err => {
      if (err.code === 'ECONNREFUSED') {
        console.info(`Connecting to ${err.address}:${err.port}`)
      } else {
        console.log('ERR', err)
      }
    })

    // this.client.on('connect', a => {
    //   console.log('connect it', a)
    // })

    this.client.on('ready', () => {
      // also set connected to false
      this.connected = true
      this.flushBuffered()
    })
  }

  async queue(
    command: string,
    args: (string | number)[],
    resolve: (x: any) => void,
    reject: (x: Error) => void,
    subscriber?: boolean
  ) {
    if (subscriber) {
      // somewhere else!
      console.info('SUBSCRIBER NOT DONE YET')
    } else {
      // // do we want to cache?
      // if (command === 'GET') {
      //   // dont execute getting the same ids
      //   // makes it a bit slower in some cases - check it
      //   const hash = fnv1a(args.join('|'))
      //   // can make this a cache
      //   const hashedRedisCommand = this.bufferedGet[hash]
      //   if (hashedRedisCommand) {
      //     if (!hashedRedisCommand.nested) {
      //       hashedRedisCommand.nested = []
      //     }
      //     hashedRedisCommand.nested.push({
      //       resolve,
      //       reject
      //     })
      //   } else {
      //     const redisCommand = {
      //       command,
      //       args,
      //       resolve,
      //       reject,
      //       hash
      //     }
      //     this.buffer.push(redisCommand)
      //     this.bufferedGet[hash] = redisCommand
      //   }
      // } else {
      this.buffer.push({
        command,
        args,
        resolve,
        reject
      })
      // }
      if (!this.inProgress && this.connected) {
        // allrdy put it inProgress, but wait 1 tick to execute it
        this.inProgress = true
        process.nextTick(() => {
          this.flushBuffered()
        })
      }
    }
  }

  private execBatch(slice: RedisCommand[]): Promise<void> {
    return new Promise((resolve, reject) => {
      const batch = this.client.batch()
      slice.forEach(({ command, args }) => {
        batch[command](...args)
      })
      batch.exec((err, reply) => {
        if (err) {
          // if set returns error then do some stuff!
          reject(err)
        } else {
          reply.forEach((v, i) => {
            slice[i].resolve(v)
            if (slice[i].nested) {
              slice[i].nested.forEach(({ resolve }) => {
                resolve(v)
              })
            }
          })
          resolve()
        }
      })
    })
  }

  private async flushBuffered() {
    this.inProgress = true
    const buffer = this.buffer
    this.buffer = []
    const len = Math.ceil(buffer.length / 5000)
    // console.log(buffer.length)
    for (let i = 0; i < len; i++) {
      const slice = buffer.slice(i * 5e3, (i + 1) * 5e3)
      await this.execBatch(slice)
    }
    if (this.buffer.length) {
      // more added over time
      await this.flushBuffered()
    }

    // make this a cache including the value
    // default 500ms or something
    // this.bufferedGet = {}
    this.inProgress = false
  }

  // subscriber stuff fix it needs to become better!
  // do it next week
  async psubscribe(...pattern: string[]): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('psubscribe', pattern, resolve, reject, true)
    })
  }

  async punsubscribe(...pattern: string[]): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('punsubscribe', pattern, resolve, reject, true)
    })
  }

  async unsubscribe(...channel: string[]): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('unsubscribe', channel, resolve, reject, true)
    })
  }

  async subscribe(...channel: string[]): Promise<any> {
    return new Promise((resolve, reject) => {
      this.queue('subscribe', channel, resolve, reject, true)
    })
  }

  async publish(channel: string, message?: string): Promise<BigInteger> {
    return new Promise((resolve, reject) => {
      this.queue('publish', [channel, message], resolve, reject, true)
    })
  }
}

// extend import Methods.ts
