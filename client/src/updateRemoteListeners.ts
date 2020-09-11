import { Callback } from './redis/types'
import { SelvaClient, RedisCommand } from '.'
import { ServerSelector } from './types'
import getServer from './getServer'
import { createConnection } from './connection'

export const addRemoteListener = (
  selvaClient: SelvaClient,
  selector: ServerSelector,
  event: string,
  cb: Callback,
  id?: string
) => {
  getServer(
    selvaClient,
    server => {
      console.log('😃 new xxxx on !', server, selvaClient.selvaId)

      const connection = createConnection(server)
      connection.attachSelvaClient(selvaClient)
      connection.addRemoteListener(
        event,
        cb,
        id === undefined ? selvaClient.selvaId : id
      )
    },
    selector
  )
}

export const removeRemoteListener = (
  selvaClient: SelvaClient,
  selector: ServerSelector,
  event: string,
  cb?: Callback,
  id?: string
) => {
  getServer(
    selvaClient,
    server => {
      console.log('😃 new gurken on !', server, selvaClient.selvaId)

      const connection = createConnection(server)
      connection.attachSelvaClient(selvaClient)
      connection.removeRemoteListener(
        event,
        cb,
        id === undefined ? selvaClient.selvaId : id
      )
    },
    selector
  )
}
