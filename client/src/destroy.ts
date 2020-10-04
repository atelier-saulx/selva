import { SelvaClient } from '.'
import { connections } from './connection'

export default async (selvaClient: SelvaClient) => {
  if (selvaClient.isDestroyed) {
    return
  }
  selvaClient.isDestroyed = true

  if (selvaClient.observables) {
    selvaClient.observables.forEach(observable => {
      observable.destroy()
    })
  }

  delete selvaClient.observables

  selvaClient.schemaObservables.forEach(v => {
    if (v.isDestroyed) {
      v.destroy()
    }
  })

  connections.forEach(connection => {
    if (connection.removeClient(selvaClient)) {
      connection.removeConnectionState(
        connection.getConnectionState(selvaClient.selvaId)
      )
    }
  })
}
