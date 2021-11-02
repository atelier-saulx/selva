import { Connection } from '.'
import { RedisCommand } from '../redis/types'
import chalk from 'chalk'

import { encodeCommand, command as strCommand } from './commands'

let uncertainStateCnt = 0
let showUncertainState = true

// here we just add  straight tcp - every command goes over this except INFO
// handle info seperately

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
      // start writing immediatly - then do per 1k
      queue.forEach(({ command, args, resolve, reject }) => {
        if (command.startsWith('selva')) {
          command = command.replace(/_/g, '.')
        }

        if (command === 'script') {
          // console.info('   COMMAND -> ', command, 'big args!')
        } else {
          // console.info('   COMMAND -> ', command, args)
        }

        // @ts-ignore
        connection.publisher.commands.push([
          resolve,
          reject,
          command,
          command === 'script' ? 'big' : args,
        ])

        const x = strCommand([command, ...args])
        // console.info('\n\nYES[', x, ']')

        connection.publisher.write(x)
      })

      // @ts-ignore
      connection.publisher.empty = (err, reply: any[]) => {
        // @ts-ignore
        delete connection.publisher.empty
        // console.info('READY')
        if (err) {
          console.error('ERROR FROM BATCH', err)

          reject(err)
        } else {
          // handle busy errors
          // let hasBusy = false

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
    }
  })
}
