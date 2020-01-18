import * as redis from 'redis'
import { createClient, RedisClient as Redis } from 'redis'
import RedisMethods from './methods'
import createIndex from './createIndex'

const redisSearchCommands = [
  'CREATE',
  'ADD',
  'ADDHASH',
  'ALTER',
  'INFO',
  'SEARCH',
  'AGGREGATE',
  'EXPLAIN',
  'DEL',
  'GET',
  'DROP',
  'SUGADD',
  'SUGGET',
  'SUGDEL',
  'SUGLEN',
  'SYNADD',
  'SYNUPDATE',
  'SYNDUMP',
  'SPELLCHECK',
  'DICTADD',
  'DICTDEL',
  'DICTDUMP',
  'CONFIG'
]

redisSearchCommands.forEach(cmd => {
  // type definition is wrong its not on the client
  // @ts-ignore
  redis.add_command(`FT.${cmd}`)
})

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
  private retryTimer: number
  private scriptShas: {
    [scriptName: string]: string
  } = {}
  private scriptBatchingEnabled: {
    [scriptSha: string]: boolean
  } = {}
  // private bufferedGet: Record<number, RedisCommand>

  constructor(connect: ConnectOptions | (() => Promise<ConnectOptions>)) {
    super()
    this.connector =
      typeof connect === 'object' ? () => Promise.resolve(connect) : connect
    this.buffer = []
    this.connect()
  }

  async loadAndEvalScript(
    scriptName: string,
    script: string,
    numKeys: number,
    keys: string[],
    args: string[],
    opts?: { batchingEnabled?: boolean }
  ): Promise<any> {
    if (!this.scriptShas[scriptName]) {
      this.scriptShas[scriptName] = await this.loadScript(script)
    }

    if (opts && opts.batchingEnabled) {
      this.scriptBatchingEnabled[this.scriptShas[scriptName]] = true
    }

    return this.evalSha(this.scriptShas[scriptName], numKeys, ...keys, ...args)
  }

  destroy() {
    this.client.quit()
    this.client = null
  }

  private async connect() {
    const opts = await this.connector()

    // even if the db does not exists should not crash!
    this.retryTimer = 100
    if (!opts.retryStrategy) {
      opts.retryStrategy = () => {
        this.connected = false
        this.connector().then(async newOpts => {
          if (newOpts.host !== opts.host || newOpts.port !== opts.port) {
            this.client.quit()
            this.connected = false
            await this.connect()
          }
        })
        if (this.retryTimer < 1e3) {
          this.retryTimer += 100
        }
        return this.retryTimer
      }
    }

    // on dc needs to re run connector - if different reconnect
    this.client = createClient(opts)

    this.client.on('error', err => {
      // console.log('ERRRRR')
      if (err.code === 'ECONNREFUSED') {
        console.info(`Connecting to ${err.address}:${err.port}`)
      } else {
        // console.log('ERR', err)
      }
    })

    this.client.on('connect', a => {
      // console.log('connect it', a)
    })

    this.client.on('ready', () => {
      createIndex(this)
      this.retryTimer = 100
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

  private batchEvalScriptArgs(origSlice: RedisCommand[]): RedisCommand[] {
    const slice: RedisCommand[] = []
    const batchedModifyArgs: { [scriptSha: string]: any[] } = {}
    const batchedModifyResolves: { [scriptSha: string]: any[] } = {}
    const batchedModifyRejects: { [scriptSha: string]: any[] } = {}

    for (const sha in this.scriptBatchingEnabled) {
      if (this.scriptBatchingEnabled[sha] === true) {
        batchedModifyArgs[sha] = []
        batchedModifyResolves[sha] = []
        batchedModifyRejects[sha] = []
      }
    }

    for (const cmd of origSlice) {
      if (cmd.command !== 'evalsha') {
        slice.push(cmd)
        continue
      }

      const scriptSha = cmd.args[0]
      if (this.scriptBatchingEnabled[scriptSha]) {
        batchedModifyArgs[scriptSha].push(...cmd.args.slice(2)) // push all args after sha and numKeys
        batchedModifyResolves[scriptSha].push(cmd.resolve)
        batchedModifyRejects[scriptSha].push(cmd.reject)
      } else {
        slice.push(cmd)
      }
    }

    for (let sha in batchedModifyArgs) {
      const modifyArgs = batchedModifyArgs[sha]
      const modifyResolves = batchedModifyResolves[sha]
      const modifyRejects = batchedModifyRejects[sha]
      if (modifyArgs.length) {
        slice.push({
          command: 'evalsha',
          args: [sha, 0, ...modifyArgs],
          resolve: (x: any) => {
            modifyResolves.forEach(resolve => resolve(x))
          },
          reject: (x: Error) => {
            modifyRejects.forEach(reject => reject(x))
          }
        })
      }
    }

    return slice
  }

  private execBatch(origSlice: RedisCommand[]): Promise<void> {
    return new Promise((resolve, reject) => {
      const batch = this.client.batch()

      const slice = Object.values(this.scriptBatchingEnabled).some(x => x)
        ? this.batchEvalScriptArgs(origSlice)
        : origSlice

      slice.forEach(({ command, args }) => {
        batch[command](...args)
      })
      batch.exec((err, reply) => {
        if (err) {
          // if set returns error then do some stuff!
          reject(err)
        } else {
          reply.forEach((v, i) => {
            if (v instanceof Error) {
              slice[i].reject(v)
              if (slice[i].nested) {
                slice[i].nested.forEach(({ reject }) => {
                  reject(v)
                })
              }
            } else {
              slice[i].resolve(v)
              if (slice[i].nested) {
                slice[i].nested.forEach(({ resolve }) => {
                  resolve(v)
                })
              }
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
}

// extend import Methods.ts
