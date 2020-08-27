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

import { REGISTRY_UPDATE } from '../constants'

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
      registryConnection.subscribe(REGISTRY_UPDATE, selvaClient.selvaId)

      registryConnection.addRemoteListener('message', (channel, msg) => {
        console.log('yesh', channel)
        if (channel === REGISTRY_UPDATE) {
          const payload = JSON.parse(msg)
          const { event } = payload
          // if (cahh)
          if (event === 'new') {
            console.log('GOT A NEW SERVER', payload)
          } else if (channel === 'remove') {
            console.log('REMOVE SERVER')
          } else if (channel === 'move-sub') {
            console.log('MOVE SUBSCRIPTION')
          } else if ('update-index') {
            console.log('update index')
            // can be either a subs manager update of index or replica
          }
        }
      })

      // add listeners

      console.log('ok made start of registry connection')
    }
  }
}
