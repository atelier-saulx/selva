import { SelvaClient, ConnectOptions } from '..'
import { createConnection } from '../connection'

/*
 registry-update
  events
  'new-server'
  'remove-server'
  'move-subscription'

  registry-server-info
    sends updates of all info objects (make this specific as well)
*/

export default (selvaClient: SelvaClient, connectOptions: ConnectOptions) => {
  console.log('_ _ _ _ connect make options do it')
  if (connectOptions instanceof Promise) {
  } else if (typeof connectOptions === 'function') {
  } else {
    const { port = 6379, host = '0.0.0.0' } = connectOptions

    if (selvaClient.registryConnection) {
      console.log('update existing connection to registry')
    } else {
      const registryConnection = createConnection({
        type: 'registry',
        name: 'registry',
        port,
        host
      })

      selvaClient.registryConnection = registryConnection

      console.log('SUBSCRIBE')
      registryConnection.subscribe('registry-update', selvaClient.selvaId)

      registryConnection.on('message', channel => {
        console.log(channel)
        // if (cahh)
        if (channel === 'new-server') {
        } else if (channel === 'remove-server') {
        } else if (channel === 'move-subscription') {
        } else if ('index-changed') {
          console.log('move dat bitch')
          // can be either a subs manager update of index or replica
        }
      })

      // add listeners

      console.log('ok made start of registry connection')
    }
  }
}
