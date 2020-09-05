import { Connection, createConnection } from './connection'
import { SelvaClient } from '.'
import { wait } from './util'

export default async (selvaClient: SelvaClient, connection: Connection) => {
  const serverDescriptor = connection.serverDescriptor

  console.log('🥞 Go somethign is hard dc arrived at selva client!')

  const state = connection.getConnectionState(selvaClient.selvaId)

  console.log(
    'my hdc state',
    state.queue.length,
    state.isEmpty,
    state.subscribes,
    state.pSubscribes,
    state.listeners
  )

  // eases timing between remove and
  await wait(10)

  if (
    selvaClient.server &&
    selvaClient.server.port === serverDescriptor.port &&
    selvaClient.server.host === serverDescriptor.host
  ) {
    // double check if isDestroyed
    console.log('new conn on server!', selvaClient.server)

    // if server is destroyed dont reconnect....
    const newConnection = createConnection(serverDescriptor)
    newConnection.attachSelvaClient(selvaClient)

    // apply state here!!!!!!!!!!

    // yesh
  } else {
    const newDescriptor = await selvaClient.getServer({
      type: serverDescriptor.type,
      name: serverDescriptor.name
    })

    console.log('RES try...', selvaClient.servers)
    console.log('go new descriptor!!!!!!', newDescriptor, serverDescriptor)

    // remove port and and host to re-apply
  }
}
