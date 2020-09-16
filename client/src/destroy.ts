import { SelvaClient } from '.'
import { connections } from './connection'

export default async (selvaClient: SelvaClient) => {
  if (selvaClient.isDestroyed) {
    return
  }
  selvaClient.isDestroyed = true
  connections.forEach(connection => {
    if (connection.removeClient(selvaClient)) {
      connection.removeConnectionState(
        connection.getConnectionState(selvaClient.selvaId)
      )
    }
  })
}
