import { Connection } from '.'
import { RedisCommand } from '../redis/types'

export default function execBatch(
  connection: Connection,
  queue: RedisCommand[]
): Promise<void> {
  return new Promise((resolve, reject) => {
    if (connection.serverIsBusy) {
      console.log('Server is busy - retrying in 5 seconds')
      connection.emit('busy')
      setTimeout(() => {
        connection.serverIsBusy = false
        if (!connection.connected) {
          console.log('DC while busy add to buffer again!')
          connection.queue.push(...queue)
        } else {
          execBatch(connection, queue)
            .then(() => {
              resolve()
            })
            .catch(err => reject(err))
        }
      }, 5e3)
    } else {
      const batch = connection.publisher.batch()

      queue.forEach(({ command, args }) => {
        if (!batch[command]) {
          throw new Error(`Command "${command}" is not a valid redis command!`)
        } else {
          batch[command](...args)
        }
      })
      batch.exec((err: Error, reply: any[]) => {
        if (err) {
          reject(err)
        } else {
          let hasBusy = false
          let busySlice = []
          reply.forEach((v: any, i: number) => {
            if (v instanceof Error) {
              if (v.message.indexOf('BUSY') !== -1) {
                hasBusy = true
                busySlice.push(queue[i])
              } else if (queue[i].reject) {
                queue[i].reject(v)
              } else {
                console.error('Error executing command', queue[i], v)
              }
            } else if (queue[i].resolve) {
              queue[i].resolve(v)
            }
          })
          if (hasBusy) {
            connection.serverIsBusy = true
            console.log('exec it again from busy')
            execBatch(connection, busySlice)
              .then(() => {
                resolve()
              })
              .catch(err => reject(err))
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
