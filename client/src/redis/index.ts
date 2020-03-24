import Observable from '../observe/observable'
import { GetOptions } from '../get/types'
import * as redis from 'redis'
import { createClient, RedisClient as Redis } from 'redis'
import RedisMethods from './methods'
import SelvaPubSub from '../pubsub'
import { createRedisClientWrapper, RedisClientWrapper } from '../redisConnector'

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
  retryStrategy?: () => number
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

let cnt = 0

export default class RedisClient extends RedisMethods {
  public connector: () => Promise<ConnectOptions>
  public client: Redis
  private buffer: RedisCommand[]
  public connected: boolean
  public id: number
  private inProgress: boolean
  public isDestroyed: boolean
  public clientWrapper: RedisClientWrapper
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
    this.id = cnt++
    this.connector =
      typeof connect === 'object' ? () => Promise.resolve(connect) : connect
    this.buffer = []
    this.connect()

    // pass connector
    this.subscriptionManager = new SelvaPubSub()
  }

  destroy() {
    this.isDestroyed = true
    this.clientWrapper.remove(this.id)
    this.subscriptionManager.destroy()
    this.client = null
  }

  private async connect() {
    if (this.isDestroyed) {
      return
    }
    this.clientWrapper = await createRedisClientWrapper(this)
    this.client = this.clientWrapper.client
    this.subscriptionManager.connect(this.clientWrapper)
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

  subscribe<T>(channel: string, getOpts: GetOptions): Observable<T> {
    return this.subscriptionManager.subscribe(channel, getOpts)
  }

  async queue(
    command: string,
    args: (string | number)[],
    resolve: (x: any) => void,
    reject: (x: Error) => void,
    subscriber?: boolean,
    type?: string // need this for smart batching GET can be cached a little bit etc
  ) {
    if (subscriber) {
      console.info('SUBSCRIBER NOT DONE YET')
    } else {
      this.buffer.push({
        command,
        args,
        resolve,
        reject
      })
      if (!this.inProgress && this.connected) {
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
