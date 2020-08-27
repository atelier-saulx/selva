import { SelvaClient, ConnectOptions, ServerDescriptor } from '..'
import { createConnection } from '../connection'
import { REGISTRY_UPDATE } from '../constants'
import getInitialRegistryServers from './getInitialRegistryServers'
import addServer from './addServer'

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
  if (connectOptions instanceof Promise) {
    // do shit
  } else if (typeof connectOptions === 'function') {
    // do shit also
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

      registryConnection.subscribe(REGISTRY_UPDATE, selvaClient.selvaId)

      getInitialRegistryServers(selvaClient).then(v => {
        console.log('yesh')
      })

      registryConnection.addRemoteListener('message', (channel, msg) => {
        if (channel === REGISTRY_UPDATE) {
          const payload = JSON.parse(msg)
          const { event } = payload
          if (event === 'new') {
            const { server } = payload
            addServer(selvaClient, <ServerDescriptor>server)
          } else if (channel === 'remove') {
            console.log('REMOVE SERVER')
          } else if (channel === 'move-sub') {
            console.log('MOVE SUBSCRIPTION')
          } else if ('update-index') {
            // can be either a subs manager update of index or replica
            console.log('update index')
          }
        }
      })

      // add listeners
      console.log('ok made start of registry connection')
    }
  }
}
