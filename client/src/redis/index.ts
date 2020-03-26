import Observable from '../observe/observable'
import { GetOptions } from '../get/types'
import * as redis from 'redis'
import { createClient, RedisClient as Redis } from 'redis'
import RedisMethods from './methods'
import SelvaPubSub from '../pubsub'

export const MAX_BATCH_SIZE = 5000

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
  'TAGVALS',
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
  type?: string
  args: (string | number)[]
  hash?: number
  nested?: Resolvable[]
}

export default class RedisClient extends RedisMethods {
  private connector: () => Promise<ConnectOptions>
  public client: Redis
  private buffer: RedisCommand[]
  private connected: boolean
  private inProgress: boolean
  private isDestroyed: boolean
  private retryTimer: number
  private scriptShas: {
    [scriptName: string]: string
  } = {}
  private scriptBatchingEnabled: {
    [scriptSha: string]: boolean
  } = {}
  public subscriptionManager: SelvaPubSub
  // private bufferedGet: Record<number, RedisCommand>

  constructor(connect: ConnectOptions | (() => Promise<ConnectOptions>)) {
    super()
    this.connector =
      typeof connect === 'object' ? () => Promise.resolve(connect) : connect
    this.buffer = []
    this.connect()

    this.subscriptionManager = new SelvaPubSub()
  }

  private resetScripts() {
    this.scriptBatchingEnabled = {}
    this.scriptShas = {}
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
      const r = await this.loadScript(script)
      this.scriptShas[scriptName] = r
    }

    if (opts && opts.batchingEnabled) {
      this.scriptBatchingEnabled[this.scriptShas[scriptName]] = true
    }

    return this.evalSha(this.scriptShas[scriptName], numKeys, ...keys, ...args)
  }

  destroy() {
    this.isDestroyed = true
    if (this.client) {
      this.client.quit()
      this.client = null
    } else {
      this.client = null
    }

    this.subscriptionManager.disconnect()
  }

  private async connect() {
    const opts = await this.connector()

    if (this.isDestroyed) {
      return
    }
    // even if the db does not exists should not crash!
    this.retryTimer = 100
    let tries = 0
    if (!opts.retryStrategy) {
      opts.retryStrategy = () => {
        // console.log('RECON', tries)
        tries++
        // needs to re do client
        // prob want a keep alive thing in here

        this.resetScripts()
        this.connected = false
        this.subscriptionManager.markSubscriptionsClosed()
        this.connector().then(async newOpts => {
          if (
            newOpts.host !== opts.host ||
            newOpts.port !== opts.port ||
            tries > 15
          ) {
            // console.log('HARD RECONN')
            this.client.quit()
            this.connected = false
            this.subscriptionManager.disconnect()
            await this.connect()
          }
        })
        if (this.retryTimer < 1e3) {
          this.retryTimer += 100
        }
        return this.retryTimer
      }
    }

    // reconnecting

    this.subscriptionManager.connect(opts)
    // on dc needs to re run connector - if different reconnect
    this.client = createClient(opts)

    this.client.on('error', err => {
      // console.log('ERR', err)
      if (err.code === 'ECONNREFUSED') {
        console.info(`Connecting to ${err.address}:${err.port}`)
      } else {
        // console.log('ERR', err)
      }
    })

    this.client.on('connect', _ => {
      tries = 0
      // console.log('connect it')
    })

    this.client.on('ready', () => {
      // console.log('ready')
      tries = 0
      this.retryTimer = 100
      this.connected = true
      this.flushBuffered()
    })
  }

  subscribe<T>(channel: string, getOpts: GetOptions): Observable<T> {
    return this.subscriptionManager.subscribe(channel, getOpts)
  }

  async queue(
    command: string,
    args: (string | number)[],
    resolve: (x: any) => void,
    reject: (x: Error) => void,
    subscriber?: boolean,
    type?: string // need this for smart batching
  ) {
    // NEED TYPE - REMOVE SUBSCRIBER

    // not good...
    if (subscriber) {
      // somewhere else!
      console.info('SUBSCRIBER NOT DONE YET')
    } else {
      // // do we want to cache?
      // batch gets pretty nice to do
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

    // should not be just modify
    for (let sha in batchedModifyArgs) {
      const modifyArgs = batchedModifyArgs[sha]
      const modifyResolves = batchedModifyResolves[sha]
      const modifyRejects = batchedModifyRejects[sha]
      if (modifyArgs.length) {
        slice.push({
          // add type
          type: 'modify',
          command: 'evalsha',
          args: [sha, 0, ...modifyArgs],
          resolve: (x: any) => {
            modifyResolves.forEach((resolve, i: number) => resolve(x[i]))
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
          console.error(err)
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
    const len = Math.ceil(buffer.length / MAX_BATCH_SIZE)
    console.log(buffer.length)
    for (let i = 0; i < len; i++) {
      console.log('batch', i)
      const slice = buffer.slice(i * MAX_BATCH_SIZE, (i + 1) * MAX_BATCH_SIZE)
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
