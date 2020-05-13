import { RedisCommand } from '../types'
import './redisSearch'
import { ServerType } from '../../types'
import execBatch from './execBatch'
import { getScriptSha } from './scripts'
import * as constants from '../../constants'

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
              // console.log('EVALSHA', redisCommand)
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

        execBatch(client, parsedQ).finally(() => {
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
      } else {
        client.queueInProgress = false
        console.log('Not connected wait a little bit')
      }
    })
  }
}

export default drainQueue
