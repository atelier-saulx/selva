import { Client } from '.'
import { RedisCommand } from '../types'

export default function execBatch(
  client: Client,
  queue: RedisCommand[]
): Promise<void> {
  return new Promise((resolve, reject) => {
    // return resolve()
    console.log('exec batch!')
    if (client.serverIsBusy) {
      console.log('Server is busy - retrying in 5 seconds')
      client.emit('busy')
      setTimeout(() => {
        client.serverIsBusy = false
        if (!client.connected) {
          console.log('DC while busy add to buffer again!')
          client.queue.push(...queue)
        } else {
          execBatch(client, queue)
            .then(() => {
              resolve()
            })
            .catch(err => reject(err))
        }
      }, 5e3)
    } else {
      const batch = client.publisher.batch()
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
            client.serverIsBusy = true
            execBatch(client, busySlice)
              .then(() => {
                console.log('exec batch from busy! DONE')

                resolve()
              })
              .catch(err => reject(err))
          } else {
            client.serverIsBusy = false
            if (queue.length > 1e3) {
              process.nextTick(() => {
                // let it gc a bit
                console.log('exec batch! DONE large')

                resolve()
              })
            } else {
              console.log('exec batch! DONE NORMAL!')

              resolve()
            }
          }
        }
      })
    }
  })
}
