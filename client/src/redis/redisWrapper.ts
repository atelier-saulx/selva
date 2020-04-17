import { createClient as createRedisClient, RedisClient } from 'redis'
import { ConnectOptions } from './'
import { v4 as uuid } from 'uuid'
import { GetOptions } from '../get/types'
import { ClientObject, RedisCommand } from './types'
import prefixes from '../prefixes'

const redisClients: Record<string, RedisWrapper> = {}
const HEARTBEAT_TIMER = 5e3

const isEmpty = (obj: { [k: string]: any }): boolean => {
  for (const _k in obj) {
    return false
  }
  return true
}

export type ClientType = 'sub' | 'client' | 'sSub' | 'sClient'

export const clientTypes: ClientType[] = ['sub', 'client', 'sSub', 'sClient']

// sSub means sub manager sub client

export class RedisWrapper {
  public client: RedisClient
  public sub: RedisClient
  public sSub: RedisClient
  public sClient: RedisClient

  public id: string
  public uuid: string
  public noSubscriptions: boolean = false
  public clients: Map<string, ClientObject> = new Map()
  public opts: ConnectOptions
  public heartbeatTimout: NodeJS.Timeout
  public types: ClientType[]
  public isDestroyed: boolean = false

  public buffer: { [client: string]: RedisCommand[] } = {}

  public inProgress: {
    [client: string]: boolean | undefined
  } = {}

  public connected: {
    [client: string]: boolean | undefined
  } = {}

  public scriptBatchingEnabled: {
    [client: string]: { [scriptSha: string]: boolean }
  } = {}

  public scriptShas: { [client: string]: { [scriptName: string]: string } } = {}

  public isBusy: { [client: string]: boolean } = {}

  public subscriptions: Record<
    string,
    {
      getOptions: GetOptions
      clients: Set<string>
      version?: string
      // QUESTION: keep cache data :/ dont know ?- have to think about this!
    }
  > = {}

  private retryTimer: number = 100

  // this exists in the case where a client reconnects faster then the server timeout
  // and it still needs to remove the subs
  public removeSubscriptionsSet: Set<string> = new Set()

  constructor(opts: ConnectOptions, id: string) {
    this.opts = opts
    this.id = id
    // to send to the server
    this.uuid = uuid()

    // subscription manager client // pub channel and setting your subs stuff
    this.types = clientTypes

    this.types.forEach(type => {
      this.scriptShas[type] = {}
      this.scriptBatchingEnabled[type] = {}
      this.connected[type] = false
      this.inProgress[type] = false
      this.buffer[type] = []
      // initialize
    })

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
    clearTimeout(this.heartbeatTimout)

    const setHeartbeat = () => {
      if (this.connected.sClient) {
        this.sClient.hget(prefixes.clients, this.uuid, (err, r) => {
          if (!err && r) {
            if (Number(r) < Date.now() - HEARTBEAT_TIMER * 5) {
              console.log('Client timedout - re send subscriptions')
              this.sendSubcriptions()
            }
          }
        })
        this.sClient.publish(
          prefixes.heartbeat,
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
      this.sub.unsubscribe(prefixes.serverHeartbeat)
    }
    clearTimeout(this.heartbeatTimout)
  }

  emitChannel(channel: string, client?: string) {
    this.queue(
      'hmget',
      [prefixes.cache, channel, channel + '_version'],
      ([data, version]) => {
        if (data) {
          const obj = JSON.parse(data)
          obj.version = version
          if (client) {
            const clientObj = this.clients.get(client)
            clientObj.message(channel, obj)
          } else {
            if (this.subscriptions[channel]) {
              this.subscriptions[channel].clients.forEach(client => {
                const clientObj = this.clients.get(client)
                clientObj.message(channel, obj)
              })
            }
          }
        }
      },
      err => {
        // @ts-ignore
        if (err.code !== 'UNCERTAIN_STATE') {
          console.error(err.message)
        }
      },
      'sClient'
    )
  }

  addListeners() {
    this.sSub.subscribe(prefixes.serverHeartbeat)

    this.sub.on('message', (channel, msg) => {
      if (channel.startsWith(prefixes.log)) {
        const [_, id] = channel.split(':')
        this.emit('log', JSON.parse(msg), id)
      }
    })

    this.sSub.on('message', (channel, msg) => {
      if (channel === prefixes.serverHeartbeat) {
        const ts: number = Number(msg)
        // FIXME: dirty when clock mismatch  - ok for now
        if (ts < Date.now() - 60 * 1e3) {
          console.log(
            '1 minute delay to get a server heartbeat - must be broken - reconnect'
          )
          this.reconnect()
        }
      } else {
        if (
          channel.indexOf('heartbeat') === -1 &&
          channel !== prefixes.remove &&
          channel !== prefixes.new
        ) {
          if (this.subscriptions[channel]) {
            this.emitChannel(channel)
          }
        }
      }
    })
  }

  cleanUp() {
    if (this.sSub) {
      this.sSub.removeAllListeners('message')
      // this.unsubscribeAllChannels()
    }
    if (this.sub) {
      this.sub.removeAllListeners('message')
    }
    this.stopHeartbeat()
    this.stopClientLogging()
    this.inProgress = {}
    this.isBusy = {}
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
      let opts = this.opts
      const { subscriptions } = opts

      if (subscriptions) {
        if (type === 'sClient' || type === 'sSub') {
          opts = subscriptions
        }
      }

      const typeOpts = Object.assign({}, opts, {
        retryStrategy: () => {
          console.log('RETRY connect', type)
          if (tries > 100) {
            console.log('node client is broken - restart', type)
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
          tries++
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
        this.isBusy[type] = false
        // ----------------------------------------------------
        client.removeAllListeners('message')
        client.removeAllListeners('pmessage')
        // ----------------------------------------------------
        this.resetScripts(type)
        this.flushBuffered(type)

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
          // this.emit('error', err)
        }
      })
    })
  }

  startClientLogging() {
    this.clients.forEach((client, id) => {
      if (client.log && this.sub) {
        this.sub.subscribe(`${prefixes.log}:${id}`)
      }
    })
  }

  stopClientLogging() {
    this.clients.forEach((client, id) => {
      if (client.log && this.sub) {
        this.sub.unsubscribe(`${prefixes.log}:${id}`)
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

  unsubscribe(client: string, channel: string) {
    if (this.subscriptions[channel]) {
      this.subscriptions[channel].clients.delete(client)
      if (this.subscriptions[channel].clients.size === 0) {
        delete this.subscriptions[channel]
        if (this.allConnected) {
          this.unsubscribeChannel(channel)
        } else {
          this.removeSubscriptionsSet.add(channel)
          if (this.sSub) {
            this.sSub.unsubscribe(channel)
          }
        }
      }
    }
  }

  unsubscribeAllChannels() {
    for (const channel in this.subscriptions) {
      if (this.sSub) {
        this.sSub.unsubscribe(channel)
      }
      this.removeSubscriptionsSet.add(channel)
    }
  }

  unsubscribeChannel(channel: string) {
    this.queue('srem', [channel, this.uuid], undefined, undefined, 'sClient')
    this.queue(
      'publish',
      [prefixes.remove, JSON.stringify({ client: this.uuid, channel })],
      () => this.removeSubscriptionsSet.delete(channel),
      undefined,
      'sClient'
    )
    this.sub.unsubscribe(channel)
  }

  subscribeChannel(channel: string, getOptions: GetOptions) {
    this.queue(
      'hsetnx',
      [prefixes.subscriptions, channel, JSON.stringify(getOptions)],
      undefined,
      undefined,
      'sClient'
    )
    this.queue('sadd', [channel, this.uuid], () => {}, undefined, 'sClient')

    this.queue(
      'publish',
      [prefixes.new, JSON.stringify({ client: this.uuid, channel })],
      undefined,
      undefined,
      'sClient'
    )

    this.sSub.subscribe(channel)
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
        this.sub.unsubscribe(`${prefixes.log}:${client}`)
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
        this.sub.subscribe(`${prefixes.log}:${client}`)
      }
    } else {
      throw new Error('trying to add a client thats allready added!')
    }
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
    type?: ClientType
  ) {
    // remove type
    if (type === undefined) {
      if (command === 'publish') {
        type = 'sClient'
      } else if (command === 'subscribe') {
        type = 'sSub'
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

  resetScripts(type: string) {
    this.scriptBatchingEnabled[type] = {}
    this.scriptShas[type] = {}
  }

  execBatch(origSlice: RedisCommand[], type: ClientType): Promise<void> {
    return new Promise((resolve, reject) => {
      if (this.isBusy[type]) {
        console.log('Server is busy - retrying in 5 seconds')
        setTimeout(() => {
          this.isBusy[type] = false
          // need to rerun the batch ofc
          if (!this.connected[type]) {
            console.log('DC while busy add to buffer again!')
            this.buffer[type].push(...origSlice)
            // add this
          } else {
            this.execBatch(origSlice, type).then(() => {
              resolve()
            })
          }
        }, 5e3)
      } else {
        // dont need this type
        const batch = this[type].batch()

        const slice = isEmpty(this.scriptBatchingEnabled[type])
          ? origSlice
          : this.batchEvalScriptArgs(origSlice, type)

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
            // console.error(err)
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
              this.isBusy[type] = true
              this.execBatch(origSlice, type).then(v => {
                resolve()
              })
            } else {
              this.isBusy[type] = false
              if (slice.length > 1e3) {
                process.nextTick(() => {
                  // let it gc a bit
                  resolve()
                })
              } else {
                resolve()
              }
            }
          }
        })
      }
    })
  }

  logBuffer(buffer, type) {
    console.log('--------------------------')
    const bufferId = (~~(Math.random() * 10000)).toString(16)
    console.log(type, 'buffer', bufferId)
    buffer.forEach(({ command, args }) => {
      if (command === 'evalsha') {
        for (let key in this.scriptShas[type]) {
          if (this.scriptShas[type][key] === args[0]) {
            console.log(` ${key}`)
            try {
              const j = JSON.parse(args[3])
              console.log(j)
            } catch (_err) {
              console.log(`    ${args}`)
            }
          }
        }
      } else {
        console.log(` ${command}`)
        if (command !== 'script') {
          console.log(`    ${args}`)
        }
      }
    })
    return bufferId
  }

  async flushBuffered(type: ClientType) {
    if (this.connected[type]) {
      if (!this.inProgress[type]) {
        this.inProgress[type] = true
        const buffer = this.buffer[type]
        if (buffer.length) {
          if (buffer.length > 1e5) {
            console.warn('buffer is larger then 100k may need to do something')
          }
          // this.logBuffer(buffer, type)
          this.buffer[type] = []
          const len = Math.ceil(buffer.length / 5000)
          for (let i = 0; i < len; i++) {
            const slice = buffer.slice(i * 5e3, (i + 1) * 5e3)
            if (!this.connected[type]) {
              // re add slice
              this.buffer[type] = buffer.slice(i * 5e3)
              return
            } else {
              if (slice.length) {
                await this.execBatch(slice, type)
                // console.log('EXECUTED BATCH', bId, type)
              }
            }
          }
          if (this.buffer[type].length) {
            this.inProgress[type] = false
            await this.flushBuffered(type)
          }
        }
        this.inProgress[type] = false
      }
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
