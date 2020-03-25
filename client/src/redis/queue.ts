import { RedisCommand } from './types'
import RedisMethods from './redisMethods'
import { RedisWrapper } from './redisWrapper'

abstract class RedisQueue extends RedisMethods {
  public redis: RedisWrapper

  public buffer: { client: RedisCommand[]; sub: RedisCommand[] } = {
    client: [],
    sub: []
  }
  public inProgress: { client: boolean; sub: boolean } = {
    client: false,
    sub: false
  }

  public connected: { client: boolean; sub: boolean } = {
    client: false,
    sub: false
  }

  public scriptBatchingEnabled: {
    sub: {
      [scriptSha: string]: boolean
    }
    client: {
      [scriptSha: string]: boolean
    }
  } = { sub: {}, client: {} }

  public scriptShas: {
    sub: {
      [scriptName: string]: string
    }
    client: {
      [scriptName: string]: string
    }
  } = { sub: {}, client: {} }

  resetScripts(type: 'client' | 'sub') {
    this.scriptBatchingEnabled[type] = {}
    this.scriptShas[type] = {}
  }

  async loadAndEvalScript(
    scriptName: string,
    script: string,
    numKeys: number,
    keys: string[],
    args: string[],
    type: 'client' | 'sub',
    opts?: { batchingEnabled?: boolean }
  ): Promise<any> {
    if (!this.scriptShas[type][scriptName]) {
      const r = await this.loadScript(script)
      this.scriptShas[type][scriptName] = r
    }
    if (opts && opts.batchingEnabled) {
      this.scriptBatchingEnabled[type][this.scriptShas[type][scriptName]] = true
    }
    return this.evalSha(
      this.scriptShas[type][scriptName],
      numKeys,
      ...keys,
      ...args
    )
  }

  async queue(
    command: string,
    args: (string | number)[],
    resolve: (x: any) => void = () => {},
    reject: (x: Error) => void = () => {},
    type?: 'client' | 'sub'
  ) {
    if (type === undefined) {
      if (command === 'subscribe') {
        type = 'sub'
      } else {
        type = 'client'
      }
    }
    this.buffer[type].push({
      command,
      args,
      resolve,
      reject
    })
    if (!this.inProgress[type] && this.connected[type]) {
      this.inProgress[type] = true
      process.nextTick(() => {
        this.flushBuffered(type)
      })
    }
  }

  batchEvalScriptArgs(origSlice: RedisCommand[]): RedisCommand[] {
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

  execBatch(
    origSlice: RedisCommand[],
    type: 'sub' | 'client' = 'client'
  ): Promise<void> {
    return new Promise((resolve, reject) => {
      const batch = this.redis[type].batch()
      const slice = Object.values(this.scriptBatchingEnabled).some(x => x)
        ? this.batchEvalScriptArgs(origSlice)
        : origSlice

      slice.forEach(({ command, args }) => {
        batch[command](...args)
      })
      batch.exec((err, reply) => {
        if (err) {
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

  public async flushBuffered(type: 'sub' | 'client') {
    if (this.connected[type]) {
      this.inProgress[type] = true
      const buffer = this.buffer[type]
      this.buffer[type] = []
      const len = Math.ceil(buffer.length / 5000)
      for (let i = 0; i < len; i++) {
        const slice = buffer.slice(i * 5e3, (i + 1) * 5e3)
        if (!this.connected[type]) {
          this.inProgress[type] = false
          return
        }
        await this.execBatch(slice, type)
      }
      if (this.buffer[type].length) {
        await this.flushBuffered(type)
      }
      this.inProgress[type] = false
    } else {
      this.inProgress[type] = false
    }
  }
}

export default RedisQueue
