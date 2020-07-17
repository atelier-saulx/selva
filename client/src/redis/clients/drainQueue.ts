import { RedisCommand } from '../types'
import execBatch from './execBatch'
import { getScriptSha, loadScripts } from './scripts'
import * as constants from '../../constants'
import { Client, addCommandToQueue } from './'

const errListener = (client: Client, redisCommand: RedisCommand, err: any) => {
  if (err) {
    console.error('Error', err)
    process.nextTick(() => {
      addCommandToQueue(client, redisCommand)
    })
  }
}

const drainQueue = (client: Client, q?: RedisCommand[]) => {
  if (!client.queueInProgress) {
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
        client.queueBeingDrained = q
        let nextQ: RedisCommand[]
        const parsedQ = []
        for (let i = 0; i < q.length; i++) {
          const redisCommand = q[i]
          const { command, resolve, args, reject } = redisCommand
          if (command === 'info') {
            client.publisher.info((err, data) => {
              if (err || !data) {
                if (reject) {
                  reject(err || new Error('no data'))
                } else if (resolve) {
                  resolve('')
                }
              } else {
                if (resolve) {
                  resolve(data)
                }
              }
            })
          } else if (command === 'punsubscribe') {
            delete client.redisSubscriptions.psubscribe[args[0]]
            client.subscriber.punsubscribe(...(<string[]>args), err =>
              errListener(client, redisCommand, err)
            )
            if (resolve) resolve(true)
          } else if (command === 'unsubscribe') {
            delete client.redisSubscriptions.subscribe[args[0]]
            client.subscriber.unsubscribe(...(<string[]>args), err =>
              errListener(client, redisCommand, err)
            )
            if (resolve) resolve(true)
          } else if (command === 'subscribe') {
            client.redisSubscriptions.subscribe[args[0]] = true
            client.subscriber.subscribe(...(<string[]>args), err =>
              errListener(client, redisCommand, err)
            )
            if (resolve) resolve(true)
          } else if (command === 'psubscribe') {
            client.redisSubscriptions.psubscribe[args[0]] = true
            client.subscriber.psubscribe(...(<string[]>args), err =>
              errListener(client, redisCommand, err)
            )
            if (resolve) resolve(true)
          } else {
            if (command.toLowerCase() === 'evalsha') {
              const script = args[0]
              if (
                typeof script === 'string' &&
                script.startsWith(constants.SCRIPT)
              ) {
                const sha = getScriptSha(
                  (<string>args[0]).slice(constants.SCRIPT.length + 1)
                )
                if (!sha) {
                  client.queue.push(redisCommand)
                  continue
                } else {
                  args[0] = sha
                  if (script === `${constants.SCRIPT}:modify`) {
                    if (!modify) {
                      modify = redisCommand
                    } else {
                      modify.args.push(...args.slice(2))
                    }

                    modifyResolvers.push(redisCommand.resolve)
                    modifyRejects.push(redisCommand.reject)
                    continue
                  }
                }
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
          const orig = modify
          modify.resolve = results => {
            for (let i = 0; i < modifyResolvers.length; i++) {
              if (modifyResolvers[i]) {
                modifyResolvers[i](results[i])
              }
            }
          }

          modify.reject = err => {
            if (err.stack.includes('NOSCRIPT')) {
              loadScripts(client, () => {
                orig.args[0] = getScriptSha('modify')
                addCommandToQueue(client, orig)
              })
              return
            }

            modifyRejects.forEach(reject => {
              if (reject) {
                reject(err)
              }
            })
          }

          parsedQ.push(modify)
          modify = undefined
        }

        const queueDone = () => {
          client.queueBeingDrained = []
          if (nextQ) {
            client.queueInProgress = false
            drainQueue(client, nextQ)
          } else if (client.queue.length) {
            client.queueInProgress = false
            drainQueue(client)
          } else {
            client.queueInProgress = false
          }
        }

        if (parsedQ.length === 0) {
          queueDone()
        } else {
          execBatch(client, parsedQ)
            .then(() => {
              queueDone()
            })
            .catch(err => {
              console.log('Error executing batch', err)
              client.queue.concat(client.queueBeingDrained)
              client.queueBeingDrained = []
              client.queueInProgress = false
            })
        }
      } else {
        client.queueInProgress = false
      }
    })
  }
}

export default drainQueue
