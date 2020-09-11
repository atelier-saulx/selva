import { SelvaClient, RedisCommand, connect } from '.'
import { ServerSelector } from './types'
import getServer from './getServer'
import { createConnection } from './connection'

export default (
  selvaClient: SelvaClient,
  command: RedisCommand,
  selector: ServerSelector
) => {
  if (!command.id) {
    command.id = selvaClient.selvaId
  }

  const method = command.command

  if (selvaClient.isDestroyed) {
    return
  }

  if (
    method === 'subscribe' ||
    method === 'psubscribe' ||
    method === 'unsubscribe' ||
    method === 'punsubscribe'
  ) {
    getServer(
      selvaClient,
      server => {
        if (selvaClient.isDestroyed) {
          return
        }

        if (selvaClient.selvaId === '1' && server.port === 9999) {
          console.log('q time', method, command.args)
        }

        if (typeof command.args[0] === 'string') {
          const connection = createConnection(server)
          connection.attachSelvaClient(selvaClient)
          connection[method](command.args[0], command.id)
        }
      },
      selector
    )
  } else {
    getServer(
      selvaClient,
      server => {
        if (selvaClient.isDestroyed) {
          return
        }

        if (selvaClient.selvaId === '1' && server.port === 9999) {
          console.log('q time', method, command.args)
        }

        const connection = createConnection(server)
        connection.attachSelvaClient(selvaClient)
        connection.command(command)
      },
      selector
    )
  }
}
