import { Connection } from './connection'
import { SelvaClient } from '.'

export default (selvaClient: SelvaClient, connection: Connection) => {
  console.log(
    '🥞 Go somethign is hard dc arrived at selva client!',
    connection.serverDescriptor
  )

  const state = connection.getConnectionState(selvaClient.selvaId)

  console.log('my state', state)
}
