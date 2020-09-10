import { Connection, createConnection } from './connection'
import { SelvaClient } from '.'
import getServer from './getServer'

export default (selvaClient: SelvaClient, connection: Connection) => {
  const serverDescriptor = connection.serverDescriptor

  console.log('🥞 Go somethign is hard dc arrived at selva client!')

  const state = connection.getConnectionState(selvaClient.selvaId)

  console.log(
    '  my hdc state',
    state.id,
    state.queue.length,
    state.isEmpty,
    state.subscribes,
    state.pSubscribes,
    state.listeners
  )

  // Eases timing between remove and re-trying to connect
  // especialy important if something was a replica and there are no other replicas (else it falls back to origin)
  setTimeout(
    () => {
      const gotNewConnection = newConnection => {
        newConnection.attachSelvaClient(selvaClient)
        newConnection.applyConnectionState(state)
        if (serverDescriptor.type === 'registry') {
          console.log('o o this is a registry :/ HDC make it')
          selvaClient.registryConnection = newConnection
        }
      }
      if (
        selvaClient.server &&
        selvaClient.server.port === serverDescriptor.port &&
        selvaClient.server.host === serverDescriptor.host
      ) {
        console.log('new conn on server!', selvaClient.server)
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
