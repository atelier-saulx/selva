import { Connection } from './connection'
import { SelvaClient } from '.'

export default (selvaClient: SelvaClient, connection: Connection) => {
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

  if (
    selvaClient.server &&
    selvaClient.server.port === serverDescriptor.port &&
    selvaClient.server.host === serverDescriptor.host
  ) {
    console.log(
      'OK THIS IS A HARD DC ON A CLIENT THAT CONNECTED TO A SERVER - THIS SHOULD NOT CALL GET SERVER WITHOUT A PORT AND HOST AGAIN',
      selvaClient.server
    )
  } else {
    // remove port and and host to re-apply
  }
}
