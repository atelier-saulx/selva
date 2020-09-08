import { Connection, createConnection } from './connection'
import { SelvaClient } from '.'
import { wait } from './util'

export default async (selvaClient: SelvaClient, connection: Connection) => {
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
  await wait(
    serverDescriptor.type === 'replica' &&
      !selvaClient.servers.replicas[serverDescriptor.name]
      ? 1000
      : 0
  )

  let newConnection

  if (
    selvaClient.server &&
    selvaClient.server.port === serverDescriptor.port &&
    selvaClient.server.host === serverDescriptor.host
  ) {
    console.log('new conn on server!', selvaClient.server)
    newConnection = createConnection(serverDescriptor)
  } else {
    const newDescriptor = await selvaClient.getServer({
      type: serverDescriptor.type,
      name: serverDescriptor.name
    })
    console.log('go new descriptor!!!!!!', newDescriptor, serverDescriptor)
    newConnection = createConnection(newDescriptor)
  }

  if (newConnection) {
    newConnection.attachSelvaClient(selvaClient)
    newConnection.applyConnectionState(state)
    if (serverDescriptor.type === 'registry') {
      console.log('o o this is a registry :/')
      selvaClient.registryConnection = newConnection
    }
  }
}
