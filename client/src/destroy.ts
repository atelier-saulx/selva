import { SelvaClient } from '.'
import { connections } from './connection'

export default async (selvaClient: SelvaClient) => {
  if (selvaClient.isDestroyed) {
    return
  }
  console.log('Destroy selva client')
  selvaClient.isDestroyed = true
  connections.forEach(connection => {
    if (connection.removeSelvaClient(selvaClient)) {
      connection.removeConnectionState(
        connection.getConnectionState(selvaClient.selvaId)
      )
    }
  })
}
