import { SelvaClient, RedisCommand } from '.'
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

  if (
    method === 'subscribe' ||
    method === 'psubscribe' ||
    method === 'unsubscribe' ||
    method === 'punsubscribe'
  ) {
    getServer(selvaClient, selector).then(server => {
      if (typeof command.args[0] === 'string') {
        createConnection(server)[method](command.args[0], command.id)
      }
    })
  } else {
    getServer(selvaClient, selector).then(server => {
      createConnection(server).command(command)
    })
  }
}
