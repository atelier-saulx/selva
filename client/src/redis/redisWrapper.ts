import { createClient as createRedisClient, RedisClient } from 'redis'
import { ConnectOptions } from './'
import { v4 as uuid } from 'uuid'
import { GetOptions } from '../get/types'
import { ClientObject, RedisCommand } from './types'

const redisClients: Record<string, RedisWrapper> = {}

const HEARTBEAT_TIMER = 5e3

const serverHeartbeat = '___selva_subscription:server_heartbeat'
const logPrefix = '___selva_lua_logs'

export class RedisWrapper {
  public client: RedisClient
  public sub: RedisClient
  public id: string
  public uuid: string
  public noSubscriptions: boolean = false
  public clients: Map<string, ClientObject> = new Map()
  public opts: ConnectOptions
  public heartbeatTimout: NodeJS.Timeout
  public types: string[]
  public isDestroyed: boolean = false

  public subscriptions: Record<
    string,
    {
      getOptions: GetOptions
      clients: Set<string>
      version?: string // check if version?
      // keep cache data :/ dont know - have to think about this!
      // for now we can just get (dirty nice)
    }
  > = {}

  public connected: {
    client: boolean
    sub: boolean
  } = {
    client: false,
    sub: false
  }
  private retryTimer: number = 100

  constructor(opts: ConnectOptions, id: string) {
    this.opts = opts
    this.id = id
    // to send to the server
    this.uuid = uuid()

    // subscription manager client // pub channel and setting your subs stuff
    this.types = ['sub', 'client']
    this.connect()
  }

  public get allConnected(): boolean {
    let allConnected = true
    for (let i = 0; i < this.types.length; i++) {
      if (!this.connected[this.types[i]]) {
        allConnected = false
        break
      }
    }
    return allConnected
  }

  public get allDisconnected(): boolean {
    let allDisconnected = true
    for (let i = 0; i < this.types.length; i++) {
      if (this.connected[this.types[i]]) {
        allDisconnected = false
        break
      }
    }
    return allDisconnected
  }

  startHeartbeat() {
    const setHeartbeat = () => {
      if (this.connected.client) {
        this.client.publish(
          '___selva_subscription:heartbeat',
          JSON.stringify({
            client: this.uuid,
            ts: Date.now()
          })
        )
        this.heartbeatTimout = setTimeout(setHeartbeat, HEARTBEAT_TIMER)
      }
    }
    setHeartbeat()
  }

  stopHeartbeat() {
    if (this.sub) {
      this.sub.unsubscribe(serverHeartbeat)
    }
    clearTimeout(this.heartbeatTimout)
  }

  emitChannel(channel: string, client?: string) {
    const cache = `___selva_cache`
    this.queue(
      'hmget',
      [cache, channel, channel + '_version'],
      ([data, version]) => {
        if (data) {
          const obj = JSON.parse(data)
          obj.version = version
          if (client) {
            const clientObj = this.clients.get(client)
            clientObj.message(channel, obj)
          } else {
            this.subscriptions[channel].clients.forEach(client => {
              const clientObj = this.clients.get(client)
              clientObj.message(channel, obj)
            })
          }
        }
      },
      err => {
        console.error(err)
      }
    )
  }

  addListeners() {
    this.sub.subscribe(serverHeartbeat)

    this.sub.on('message', (channel, msg) => {
      if (channel === serverHeartbeat) {
        const ts: number = Number(msg)
        // FIXME: dirty when clock mismatch  - ok for now
        if (ts < Date.now() - 60 * 1e3) {
          console.log(
            '1 minute delay to get a server heartbeat - must be broken - reconnect'
          )
          this.reconnect()
        }
      } else if (channel.indexOf(logPrefix) === 0) {
        const [_, id] = channel.split(':')
        this.emit('log', JSON.parse(msg), id)
      } else {
        if (
          channel.indexOf('heartbeat') === -1 &&
          channel !== '___selva_subscription:remove' &&
          channel !== '___selva_subscription:new'
        ) {
          if (this.subscriptions[channel]) {
            this.emitChannel(channel)
          }
        }
      }
    })

    console.log('Client listeners', 'sub', this.sub.rawListeners.length)
  }

  cleanUp() {
    if (this.sub) {
      this.sub.removeAllListeners('message')
      // this.unsubscribeAllChannels()
    }
    this.stopHeartbeat()
    this.stopClientLogging()
  }

  emit(type: string, value: any, client?: string) {
    if (!client) {
      this.clients.forEach(obj => {
        if (obj[type]) {
          obj[type](value)
        }
      })
    } else {
      const obj = this.clients.get(client)
      if (obj) {
        if (obj[type]) {
          obj[type](value)
        }
      }
    }
  }

  connect() {
    this.types.forEach(type => {
      let tries = 0
      const typeOpts = Object.assign({}, this.opts, {
        retryStrategy: () => {
          tries++
          if (tries > 100) {
            this.reconnect()
          } else {
            if (tries === 0 && this.connected[type] === true) {
              this.connected[type] = false
              if (this.allDisconnected) {
                this.cleanUp()
              }
              this.emit('disconnect', type)
            }
          }
          this.connected[type] = false
          if (this.retryTimer < 1e3) {
            this.retryTimer += 100
          }
          return this.retryTimer
        }
      })

      const client = (this[type] = createRedisClient(typeOpts))

      client.on('ready', () => {
        tries = 0
        this.connected[type] = true
        // ----------------------------------------------------
        // hopefully this does not break anything
        client.removeAllListeners('message')
        client.removeAllListeners('pmessage')
        // ----------------------------------------------------
        this.resetScripts(type)

        // initialize logging for each client
        /*
          this.redis.sub.subscribe(`${logPrefix}:${this.clientId}`)
        */
        if (this.allConnected) {
          this.startHeartbeat()
          this.sendSubcriptions()
          this.addListeners()
          this.startClientLogging()
        }
        this.emit('connect', type)
      })

      client.on('error', err => {
        if (err.code === 'ECONNREFUSED') {
          if (this.connected[type]) {
            this.connected[type] = false
            if (this.allDisconnected) {
              this.cleanUp()
            }
            this.emit('disconnect', type)
          }
        } else {
          this.emit('error', err)
        }
      })
    })
  }

  startClientLogging() {
    this.clients.forEach((client, id) => {
      if (client.log) {
        console.log('subscribe', `${logPrefix}:${id}`)
        this.sub.subscribe(`${logPrefix}:${id}`)
      }
    })
  }

  stopClientLogging() {
    this.clients.forEach((client, id) => {
      if (client.log) {
        this.sub.unsubscribe(`${logPrefix}:${id}`)
      }
    })
  }

  disconnect(noListeners: boolean = false) {
    this.cleanUp()
    this.types.forEach(type => {
      this.connected[type] = false
      if (this[type]) {
        this[type].removeAllListeners()
        this[type].quit()
        this[type] = null
      }
      if (!noListeners) {
        this.emit('disconnect', type)
      }
    })
  }

  reconnect() {
    this.disconnect()
    setTimeout(() => {
      this.connect()
    }, 1e3)
  }

  subscribe(client: string, channel: string, getOptions: GetOptions) {
    if (!this.subscriptions[channel]) {
      this.removeSubscriptionsSet.delete(channel)
      this.subscriptions[channel] = {
        clients: new Set(),
        getOptions
      }
      this.subscriptions[channel].clients.add(client)
      if (this.allConnected) {
        this.subscribeChannel(channel, getOptions)
      }
    } else {
      this.subscriptions[channel].clients.add(client)
    }
  }

  // this exists in the case where a client reconnects faster then the server timeout
  // and it still needs to remove the subs
  public removeSubscriptionsSet: Set<string> = new Set()

  unsubscribe(client: string, channel: string) {
    if (this.subscriptions[channel]) {
      this.subscriptions[channel].clients.delete(client)
      if (this.subscriptions[channel].clients.size === 0) {
        delete this.subscriptions[channel]
        if (this.allConnected) {
          this.unsubscribeChannel(channel)
        } else {
          this.removeSubscriptionsSet.add(channel)
          if (this.sub) {
            this.sub.unsubscribe(channel)
          }
        }
      }
    }
  }

  unsubscribeAllChannels() {
    for (const channel in this.subscriptions) {
      if (this.sub) {
        this.sub.unsubscribe(channel)
      }
      this.removeSubscriptionsSet.add(channel)
    }
  }

  unsubscribeChannel(channel: string) {
    const removeSubscriptionChannel = '___selva_subscription:remove'
    this.queue('srem', [channel, this.uuid])
    this.queue(
      'publish',
      [
        removeSubscriptionChannel,
        JSON.stringify({ client: this.uuid, channel })
      ],
      () => this.removeSubscriptionsSet.delete(channel)
    )
    this.sub.unsubscribe(channel)
  }

  subscribeChannel(channel: string, getOptions: GetOptions) {
    const subscriptions = '___selva_subscriptions'
    const newSubscriptionChannel = '___selva_subscription:new'
    this.queue('hsetnx', [subscriptions, channel, JSON.stringify(getOptions)])
    this.queue('sadd', [channel, this.uuid])
    this.queue('publish', [
      newSubscriptionChannel,
      JSON.stringify({ client: this.uuid, channel })
    ])
    this.sub.subscribe(channel)
    this.emitChannel(channel)
  }

  public sendSubcriptions() {
    for (const channel in this.subscriptions) {
      this.subscribeChannel(channel, this.subscriptions[channel].getOptions)
    }

    this.removeSubscriptionsSet.forEach(channel => {
      this.unsubscribeChannel(channel)
    })
  }

  public removeClient(client: string) {
    const clientObj = this.clients.get(client)
    if (clientObj) {
      this.clients.delete(client)
      for (const channel in clientObj.client.subscriptions) {
        this.unsubscribe(client, channel)
      }
      if (clientObj.log && this.connected.sub) {
        this.sub.unsubscribe(`${logPrefix}:${client}`)
      }
      delete clientObj.client
    }
    if (this.clients.size === 0) {
      this.isDestroyed = true
      this.disconnect(true)
      delete redisClients[this.id]
    }
  }

  public addClient(client: string, clientObj: ClientObject) {
    // add log here as well
    if (!this.clients.get(client)) {
      this.clients.set(client, clientObj)
      this.types.forEach(type => {
        if (this.connected[type]) {
          clientObj.connect(type)
        }
      })
      if (this.connected.sub && clientObj.log) {
        this.sub.subscribe(`${logPrefix}:${client}`)
      }
    } else {
      throw new Error('trying to add a client thats allready added!')
    }
  }

  public buffer: { client: RedisCommand[]; sub: RedisCommand[] } = {
    client: [],
    sub: []
  }

  public inProgress: { client: boolean; sub: boolean } = {
    client: false,
    sub: false
  }

  loadAndEvalScript(
    args: (string | number)[],
    type: string,
    resolve?: (x: any) => void,
    reject?: (x: Error) => void
  ) {
    const [batchingEnabled, script, scriptName, numKeys, ...realArgs] = args
    if (!this.scriptShas[type][scriptName]) {
      this.queue(
        'script',
        ['load', script],
        r => {
          this.scriptShas[type][scriptName] = r
          this.loadAndEvalScript(args, type, resolve, reject)
        },
        reject
      )
    } else {
      if (batchingEnabled) {
        this.scriptBatchingEnabled[type][
          this.scriptShas[type][scriptName]
        ] = true
      }
      this.queue(
        'evalsha',
        [this.scriptShas[type][scriptName], numKeys, ...realArgs],
        resolve,
        reject
      )
    }
  }

  async queue(
    command: string,
    args: (string | number)[],
    resolve?: (x: any) => void,
    reject?: (x: Error) => void,
    type?: string
  ) {
    // remove type
    if (type === undefined) {
      if (command === 'subscribe') {
        type = 'sub'
      } else {
        type = 'client'
      }
    }
    if (command === 'loadAndEvalScript') {
      this.loadAndEvalScript(args, type, resolve, reject)
    } else {
      this.buffer[type].push({
        command,
        args,
        resolve,
        reject
      })
    }

    if (!this.inProgress[type] && this.connected[type]) {
      this.inProgress[type] = true
      process.nextTick(() => {
        this.flushBuffered(type)
      })
    }
  }

  batchEvalScriptArgs(origSlice: RedisCommand[], type: string): RedisCommand[] {
    // remove type
    const slice: RedisCommand[] = []
    const batchedModifyArgs: { [scriptSha: string]: any[] } = {}
    const batchedModifyResolves: { [scriptSha: string]: any[] } = {}
    const batchedModifyRejects: { [scriptSha: string]: any[] } = {}

    for (const sha in this.scriptBatchingEnabled[type]) {
      if (this.scriptBatchingEnabled[type][sha] === true) {
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
      if (this.scriptBatchingEnabled[type][scriptSha]) {
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

  resetScripts(type: string) {
    this.scriptBatchingEnabled[type] = {}
    this.scriptShas[type] = {}
  }

  public isBusy: boolean = false

  execBatch(origSlice: RedisCommand[], type: string): Promise<void> {
    return new Promise((resolve, reject) => {
      if (this.isBusy) {
        console.log('Server is busy - retrying in 1 second')
        setTimeout(() => {
          this.isBusy = false
          this.execBatch(origSlice, type)
        }, 1e3)
      } else {
        // dont need this type
        const batch = this[type].batch()
        const slice = Object.values(this.scriptBatchingEnabled).some(x => x)
          ? this.batchEvalScriptArgs(origSlice, type)
          : origSlice

        slice.forEach(({ command, args }) => {
          if (!batch[command]) {
            throw new Error(
              `Command "${command}" is not a valid redis command!`
            )
          } else {
            batch[command](...args)
          }
        })

        batch.exec((err: Error, reply: any[]) => {
          if (err) {
            console.error(err)
            reject(err)
          } else {
            let hasBusy = false
            reply.forEach((v: any, i: number) => {
              if (v instanceof Error) {
                if (v.message.indexOf('BUSY') !== -1) {
                  hasBusy = true
                  this.queue(
                    slice[i].command,
                    slice[i].args,
                    slice[i].resolve,
                    slice[i].reject,
                    type
                  )
                } else if (slice[i].reject) {
                  slice[i].reject(v)
                } else {
                  console.error('Error executing command', slice[i], v)
                }
              } else if (slice[i].resolve) {
                slice[i].resolve(v)
              }
            })
            if (hasBusy) {
              this.isBusy = true
            } else {
              this.isBusy = false
            }
            resolve()
          }
        })
      }
    })
  }

  async flushBuffered(type: string) {
    // dont need this type
    // extra optmization is to check for the same gets / sets / requests
    // remove type
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
        } else {
          try {
            await this.execBatch(slice, type)
          } catch (err) {
            console.error(err)
          }
        }
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

export const createClient = opts => {
  const id = `${opts.host || '0.0.0.0'}:${opts.port}`
  if (redisClients[id]) {
    return redisClients[id]
  } else {
    const wrapper = new RedisWrapper(opts, id)
    redisClients[id] = wrapper
    return wrapper
  }
}
