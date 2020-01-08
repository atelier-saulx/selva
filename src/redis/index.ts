import { promisify } from 'util'
import { createClient, RedisClient as Redis, Commands } from 'redis'
import commands from './commands'

export type ConnectOptions = {
  port: number
  host?: string
  retryStrategy?: () => number // make a good default dont want to confiure this all the time
  // how we currently use this type is a 'service type' which can hold a bit more info, like name, id etc of the service)
}

type RedisCommand = {
  command: string
  args: (string | number)[]
  resolve: (x: any) => void
  reject: (x: Error) => void
}

export default class RedisClient {
  private connector: () => Promise<ConnectOptions>
  private client: Redis
  private buffer: RedisCommand[]
  private connected: boolean
  private inProgress: boolean

  constructor(connect: ConnectOptions | (() => Promise<ConnectOptions>)) {
    this.connector =
      typeof connect === 'object' ? () => Promise.resolve(connect) : connect

    this.buffer = []

    const wrapCommand = key => {
      if (!this[key]) {
        this[key] = (...args) =>
          new Promise((resolve, reject) => {
            this.queue(key, args, resolve, reject)
          })
      }
    }

    commands.forEach(wrapCommand)

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
      console.log('ERR', err)
    })

    this.client.on('connect', a => {
      console.log('connect it', a)
    })

    this.client.on('ready', () => {
      console.log('go drain')
      this.connected = true
      this.flushBuffered()
    })
  }

  private async queue(
    command: string,
    args: (string | number)[],
    resolve: (x: any) => void,
    reject: (x: Error) => void
  ) {
    this.buffer.push({
      command,
      args,
      resolve,
      reject
    })
    if (!this.inProgress && this.connected) {
      // allrdy put it inProgress
      this.inProgress = true
      process.nextTick(() => {
        this.flushBuffered()
      })
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
          reject(err)
        } else {
          reply.forEach((v, i) => {
            slice[i].resolve(v)
          })
          resolve()
        }
      })
    })
  }
  // FIXME: this is not ready
  private async flushBuffered() {
    this.inProgress = true
    const buffer = this.buffer
    this.buffer = []
    console.log('drain this', buffer)
    const len = Math.ceil(buffer.length / 5000)
    for (let i = 0; i < len; i++) {
      const slice = buffer.slice(i * 5e3, (i + 1) * 5e3)
      await this.execBatch(slice)
    }
    if (this.buffer.length) {
      await this.flushBuffered()
    }
    this.inProgress = false
  }
}
