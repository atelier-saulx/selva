import { Connection } from './connection'
import { SelvaClient } from '.'

export default (selvaClient: SelvaClient, connection: Connection) => {
  const serverDescriptor = connection.serverDescriptor

  console.log(
    '🥞 Go somethign is hard dc arrived at selva client!',
    connection.serverDescriptor
  )

  if (
    selvaClient.server &&
    selvaClient.server.port === serverDescriptor.port &&
    selvaClient.server.host === serverDescriptor.host
  ) {
    console.log(
      'OK THIS IS A HARD DC ON A CLIENT THAT CONNECTED TO A SERVER - THIS SHOULD NOT CALL GET SERVER WITHOUT A PORT AND HOST AGAIN'
    )
  }

  const state = connection.getConnectionState(selvaClient.selvaId)

  console.log('my state', state)
}
