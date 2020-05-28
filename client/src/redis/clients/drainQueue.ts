import { RedisCommand } from '../types'
import './redisSearch'
import execBatch from './execBatch'
import { getScriptSha } from './scripts'
import * as constants from '../../constants'
import { Client } from './'

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
          const { command, resolve, args } = redisCommand

          if (command === 'subscribe') {
            // console.log('yes do it subscribe it', args, client.name, client.id)
            client.subscriber.subscribe(...(<string[]>args))
            resolve(true)
          } else if (command === 'psubscribe') {
            client.subscriber.psubscribe(...(<string[]>args))
            resolve(true)
          } else {
            if (redisCommand.command.toLowerCase() === 'evalsha') {
              // console.log('EVALSHA', redisCommand)
              const script = redisCommand.args[0]

              if (
                typeof script === 'string' &&
                script.startsWith(constants.SCRIPT)
              ) {
                const sha = getScriptSha(
                  (<string>redisCommand.args[0]).slice(
                    constants.SCRIPT.length + 1
                  )
                )

                if (!sha) {
                  client.queue.push(redisCommand)
                  continue
                } else {
                  redisCommand.args[0] = sha
                  if (script === `${constants.SCRIPT}:modify`) {
                    if (!modify) {
                      modify = redisCommand
                    } else {
                      modify.args.push(...redisCommand.args.slice(2))
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
          // console.log('COMBINED', modify)
          modify.resolve = results => {
            for (let i = 0; i < modifyResolvers.length; i++) {
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

        const d = Date.now()
        execBatch(client, parsedQ)
          .then(() => {
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
          })
          .catch(err => {
            console.log('Error executing batch', err)
            client.queue.concat(client.queueBeingDrained)
            client.queueBeingDrained = []
            client.queueInProgress = false
          })
      } else {
        client.queueInProgress = false
      }
    })
  }
}

export default drainQueue
