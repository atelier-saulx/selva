import { RedisCommand } from '../redis/types'
import execBatch from './execBatch'
import { getScriptSha } from './scripts'
import * as constants from '../constants'
import { Connection } from './'

const drainQueue = (connection: Connection, q?: RedisCommand[]) => {
  if (!connection.queueInProgress) {
    connection.addActive()

    // allways scripts first!

    connection.queueInProgress = true
    process.nextTick(() => {
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
          } else if (command === 'xgroup') {
            connection.publisher['xgroup'](...args, (err, data) => {
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

        const queueDone = () => {
          connection.removeActive()

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
          if (connection.isDestroyed) {
            console.log('Connection destroyed while trying to execute queue')
          } else {
            execBatch(connection, parsedQ)
              .then(() => {
                queueDone()
              })
              .catch((err) => {
                // do clear it else the connection never gets removed!
                connection.removeActive()
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
        }
      } else {
        console.log('Connection disconnected while draining queue')
        connection.queueInProgress = false
      }
    })
  }
}

export default drainQueue
