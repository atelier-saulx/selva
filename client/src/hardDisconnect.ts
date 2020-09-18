import { Connection, createConnection } from './connection'
import { SelvaClient } from '.'
import getServer from './getServer'

export default (selvaClient: SelvaClient, connection: Connection) => {
  const serverDescriptor = connection.serverDescriptor

  const state = connection.getConnectionState(selvaClient.selvaId)

  // Eases timing between remove and re-trying to connect
  // especialy important if something was a replica and there are no other replicas (else it falls back to origin)
  setTimeout(
    () => {
      const gotNewConnection = (newConnection: Connection) => {
        newConnection.attachClient(selvaClient)
        newConnection.applyConnectionState(state)

        // emit reconnect
        selvaClient.emit('reconnect')

        if (serverDescriptor.type === 'registry') {
          selvaClient.registryConnection = newConnection
        }
      }
      if (
        selvaClient.server &&
        selvaClient.server.port === serverDescriptor.port &&
        selvaClient.server.host === serverDescriptor.host
      ) {
        gotNewConnection(createConnection(serverDescriptor))
      } else {
        getServer(
          selvaClient,
          serverDescriptor => {
            gotNewConnection(createConnection(serverDescriptor))
          },
          {
            type: serverDescriptor.type,
            name: serverDescriptor.name
          }
        )
      }
    },
    serverDescriptor.type === 'replica' &&
      !selvaClient.servers.replicas[serverDescriptor.name]
      ? 1000
      : 0
  )
}
