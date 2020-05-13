import { RedisClient } from 'redis'
import { RedisCommand } from '../types'
import RedisSelvaClient from '../'
import './redisSearch'
import { ServerType } from '../../types'
import { EventEmitter } from 'events'
import createRedisClient from './createRedisClient'
import execBatch from './execBatch'
import { loadScripts, getScriptSha } from './scripts'
import * as constants from '../../constants'

type ClientOpts = {
  name: string
  type: ServerType
  host: string
  port: number
  id: string
}

const drainQueue = (client: Client, q?: RedisCommand[]) => {
  if (!client.queueInProgress) {
    console.log('PUT IN PROGRESS')
    client.queueInProgress = true
    process.nextTick(() => {
      let modify: RedisCommand
      let modifyResolvers = []
      let modifyRejects = []

      if (client.connected) {
        if (!q) {
          q = client.queue
          client.queue = []
        }
        let nextQ: RedisCommand[]
        const parsedQ = []
        for (let i = 0; i < q.length; i++) {
          const redisCommand = q[i]
          const { command, resolve, args } = redisCommand
          if (command === 'subscribe') {
            client.subscriber.subscribe(...(<string[]>args))
            resolve(true)
          } else if (command === 'psubscribe') {
            client.subscriber.psubscribe(...(<string[]>args))
            resolve(true)
          } else {
            if (redisCommand.command.toLowerCase() === 'evalsha') {
              console.log('EVALSHA', redisCommand)
              const script = redisCommand.args[0]

              if (
                typeof script === 'string' &&
                script.startsWith(constants.SCRIPT)
              ) {
                redisCommand.args[0] = getScriptSha(
                  (<string>redisCommand.args[0]).slice(
                    constants.SCRIPT.length + 1
                  )
                )
              }

              if (script === `${constants.SCRIPT}:modify`) {
                console.log('MODIFY', q.length)
                if (!modify) {
                  modify = redisCommand
                } else {
                  console.log('HMMMMMM', ...redisCommand.args.slice(2))
                  modify.args.push(...redisCommand.args.slice(2))
                }

                modifyResolvers.push(redisCommand.resolve)
                modifyRejects.push(redisCommand.reject)
                continue
              }
            }

            parsedQ.push(redisCommand)
            if (parsedQ.length >= 5e3) {
              nextQ = q.slice(i)
              break
            }
          }
        }

        if (modify) {
          console.log('COMBINED', modify)
          modify.resolve = results => {
            for (let i = 0; i < results.length; i++) {
              modifyResolvers[i](results[i])
            }
          }

          modify.reject = err => {
            modifyRejects.forEach(reject => {
              reject(err)
            })
          }

          parsedQ.push(modify)
          modify = undefined
        }

        console.log('go batch')

        execBatch(client, parsedQ).finally(() => {
          console.log(nextQ)
          console.log(client.queue.length)
          console.log('SNURFELS Q IS DONE!')
          if (nextQ) {
            client.queueInProgress = false
            drainQueue(client, nextQ)
          } else if (client.queue.length) {
            console.log('go drain more!')
            client.queueInProgress = false
            drainQueue(client)
          } else {
            console.log('QUE OUT OF PROGRESS!')
            client.queueInProgress = false
          }
        })
      } else {
        client.queueInProgress = false
        console.log('Not connected wait a little bit')
      }
    })
  }
}

export class Client extends EventEmitter {
  public subscriber: RedisClient
  public publisher: RedisClient
  public queue: RedisCommand[]
  public queueInProgress: boolean
  public name: string // for logging
  public type: ServerType // for logs
  public id: string // url:port
  public connected: boolean
  public serverIsBusy: boolean // can be written from the registry
  public scripts: {
    batchingEnabled: { [scriptSha: string]: boolean }
    sha: { [scriptName: string]: string }
  }
  public clients: Set<RedisSelvaClient>
  public heartbeatTimout?: NodeJS.Timeout

  constructor({ name, type, host, port, id }: ClientOpts) {
    super()
    this.setMaxListeners(10000)

    this.name = name
    this.type = type
    this.id = id

    this.clients = new Set()

    this.scripts = { batchingEnabled: {}, sha: {} }

    this.serverIsBusy = false

    this.queueInProgress = false
    this.queue = []

    this.connected = false

    this.on('connect', () => {
      console.log('CONNECT!')

      this.connected = true
      drainQueue(this)
    })

    this.on('disconnect', () => {
      this.connected = false
      this.queueInProgress = false
    })

    this.subscriber = createRedisClient(this, host, port, 'subscriber')
    this.publisher = createRedisClient(this, host, port, 'publisher')
  }
}

const clients: Map<string, Client> = new Map()

// addSelvaClient  (createClient(type, id, selvaClient))
// removeSelvaClient

// addCommandToQueue
// --- in coming add command queue ---
// handle these special
// subscribe
// unsubscribe
// psubscribe
// punsubcribe

// selvaSubscribe
// selvaUnsubscribe
// loadAndEvalScript

// export type Client = {
//     subscriber: RedisClient
//     publisher: RedisClient
//     queue: RedisCommand[]

//     name: string // for logging
//     type: ServerType // for logging as well

// url: string
// port: string
//     id: string // url:port
//     connected: boolean
//     bufferInProgress: boolean
//     busy: boolean // can be written from the registry
//     heartbeatTimout: NodeJS.Timeout

//     scripts: {
//       batchingEnabled: { [scriptSha: string]: boolean }
//       sha: { [scriptName: string]: string }
//     }

//     clients: Set<Redis>
//   }

const destroyClient = () => {}

const createClient = (
  name: string,
  type: ServerType,
  id: string,
  port: number,
  host: string
): Client => {
  const client: Client = new Client({
    id,
    name,
    type,
    port,
    host
  })
  return client
}

export function removeRedisSelvaClient(
  client: Client,
  selvaRedisClient: RedisSelvaClient
) {}

export function addRedisSelvaClient(
  client: Client,
  selvaRedisClient: RedisSelvaClient
) {}

export function getClient(
  selvaRedisClient: RedisSelvaClient,
  name: string,
  type: ServerType,
  port: number,
  url: string = '0.0.0.0'
) {
  // if origin || registry

  const id = url + port
  let client = clients.get(id)
  if (!client) {
    client = createClient(name, type, id, port, url)
  }

  if (type === 'origin' || /* TODO: remove */ type === 'registry') {
    loadScripts(client)
  }

  return client
}

export function addCommandToQueue(client: Client, redisCommand: RedisCommand) {
  client.queue.push(redisCommand)
  if (!client.queueInProgress) {
    drainQueue(client)
  } else {
    console.log('o! new thing queue in  progress', redisCommand.command)
  }
}
