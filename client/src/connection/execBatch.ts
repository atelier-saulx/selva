import { Connection } from '.'
import { RedisCommand } from '../redis/types'
import chalk from 'chalk'

let uncertainStateCnt = 0
let showUncertainState = true

export default function execBatch(
  connection: Connection,
  queue: RedisCommand[]
): Promise<void> {
  return new Promise((resolve, reject) => {
    if (connection.serverIsBusy) {
      console.info('Server is busy - retrying in 5 seconds')
      connection.emit('busy')
      setTimeout(() => {
        connection.serverIsBusy = false
        if (!connection.connected) {
          console.info('DC while busy add to buffer again!')
          connection.queue.push(...queue)
        } else {
          execBatch(connection, queue)
            .then(() => {
              resolve()
            })
            .catch((err) => reject(err))
        }
      }, 5e3)
    } else {
      const batch = connection.publisher.batch()

      // console.log(queue.filter(v => v.command !== 'SCRIPT'))

      queue.forEach(({ command, args }) => {
        if (!batch[command]) {
          throw new Error(`Command "${command}" is not a valid redis command!`)
        } else {
          batch[command](...args)
        }
      })
      batch.exec((err: Error, reply: any[]) => {
        if (err) {
          console.error('ERROR FROM BATCH', err)

          reject(err)
        } else {
          let hasBusy = false

          const busySlice = []

          if (reply) {
            reply.forEach((v: any, i: number) => {
              if (v instanceof Error) {
                if (v.message.indexOf('BUSY') !== -1) {
                  hasBusy = true
                  busySlice.push(queue[i])
                } else if (queue[i].reject) {
                  if (v.message.includes('READONLY')) {
                    console.error(
                      connection.serverDescriptor,
                      'OK HERE SOMETHING WRONG (readonly)',
                      queue[i],
                      connection.serverDescriptor
                    )
                  }

                  // @ts-ignore
                  if (v.code === 'UNCERTAIN_STATE') {
                    // if publish ignore
                    // console.log(connection.queue, connection.queueBeingDrained, connection.queueInProgress)
                    if (showUncertainState) {
                      showUncertainState = false
                      uncertainStateCnt++
                      setTimeout(() => {
                        showUncertainState = true
                        console.warn(
                          chalk.yellow(
                            connection.serverDescriptor,
                            `Uncertain state errors (connection lost) fired ${uncertainStateCnt}x in the last second`
                          )
                        )
                        uncertainStateCnt = 0
                      }, 1e3)
                    }
                    // publish will be lost
                    connection.queue.push(queue[i])
                  } else {
                    // most cases here we want to treat it as a busy error
                    queue[i].reject(v)
                  }
                } else {
                  console.error('Error executing command', queue[i], v)
                }
              } else if (queue[i].resolve) {
                queue[i].resolve(v)
              }
            })
          }
          if (hasBusy) {
            connection.serverIsBusy = true
            console.info('exec it again from busy')
            execBatch(connection, busySlice)
              .then(() => {
                resolve()
              })
              .catch((err) => reject(err))
          } else {
            connection.serverIsBusy = false
            if (queue.length > 1e3) {
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
