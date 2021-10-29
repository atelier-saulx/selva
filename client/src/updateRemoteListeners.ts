import { Callback } from './redis/types'
import { SelvaClient } from '.'
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
    (server) => {
      const connection = createConnection(server)
      connection.attachClient(selvaClient)
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
    (server) => {
      const connection = createConnection(server)
      connection.attachClient(selvaClient)
      connection.removeRemoteListener(
        event,
        cb,
        id === undefined ? selvaClient.selvaId : id
      )
    },
    selector
  )
}
