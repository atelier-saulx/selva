import { RedisCommand } from '../redis/types'
import execBatch from './execBatch'
import { getScriptSha, loadScripts } from './scripts'
import * as constants from '../constants'
import { Connection } from './'

const drainQueue = (connection: Connection, q?: RedisCommand[]) => {
  if (!connection.queueInProgress) {
    connection.queueInProgress = true
    process.nextTick(() => {
      let modify: RedisCommand
      let modifyResolvers = []
      let modifyRejects = []
      if (connection.connected) {
        if (!q) {
          q = connection.queue
          connection.queue = []
        }
        connection.queueBeingDrained = q
        let nextQ: RedisCommand[]
        const parsedQ = []
        for (let i = 0; i < q.length; i++) {
          const redisCommand = q[i]
          const { command, resolve, args, reject } = redisCommand
          if (command === 'info') {
            connection.publisher.info((err, data) => {
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
                  connection.queue.push(redisCommand)
                  continue
                } else {
                  args[0] = sha
                  if (script === `${constants.SCRIPT}:modify`) {
                    let no = false
                    if (!modify) {
                      modify = redisCommand
                    } else {
                      if (modify.args.length > 2e3) {
                        if (!nextQ) {
                          nextQ = []
                        }
                        nextQ.push(redisCommand)
                        no = true
                      } else {
                        modify.args.push(...args.slice(2))
                      }
                    }
                    if (!no) {
                      modifyResolvers.push(redisCommand.resolve)
                      modifyRejects.push(redisCommand.reject)
                    }
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
              loadScripts(connection, () => {
                orig.args[0] = getScriptSha('modify')
                connection.command(orig)
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
          connection.queueBeingDrained = []
          if (nextQ) {
            connection.queueInProgress = false
            drainQueue(connection, nextQ)
          } else if (connection.queue.length) {
            connection.queueInProgress = false
            drainQueue(connection)
          } else {
            connection.queueInProgress = false
          }
        }

        if (parsedQ.length === 0) {
          queueDone()
        } else {
          execBatch(connection, parsedQ)
            .then(() => {
              queueDone()
            })
            .catch(err => {
              console.log(
                'Error executing batch',
                err,
                connection.queueBeingDrained.length
              )
              connection.queue.concat(connection.queueBeingDrained)
              connection.queueBeingDrained = []
              connection.queueInProgress = false
            })
        }
      } else {
        connection.queueInProgress = false
      }
    })
  }
}

export default drainQueue
