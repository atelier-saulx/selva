import { SelvaClient } from '.'
import { connections } from './connection'

export default async (selvaClient: SelvaClient) => {
  console.log('Destroy selva client')
  connections.forEach(connection => {
    if (connection.removeSelvaClient(selvaClient)) {
      connection.removeConnectionState(
        connection.getConnectionState(selvaClient.selvaId)
      )
    }
  })
}
