import { SelvaClient, ConnectOptions, ServerDescriptor } from '..'
import { createConnection } from '../connection'
import { REGISTRY_UPDATE } from '../constants'
import getInitialRegistryServers from './getInitialRegistryServers'
import addServer from './addServer'
import removeServer from './removeServer'

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

      selvaClient.registryConnection.on('connect', () => {
        getInitialRegistryServers(selvaClient).then(() => {
          selvaClient.emit('added-servers', { event: '*' })
        })
      })

      selvaClient.registryConnection.on('destroy', () => {
        selvaClient.servers = {
          ids: new Set(),
          origins: {},
          subsManagers: [],
          replicas: []
        }
        selvaClient.emit('removed-servers', { event: '*' })
      })

      registryConnection.addRemoteListener('message', (channel, msg) => {
        if (channel === REGISTRY_UPDATE) {
          const payload = JSON.parse(msg)
          const { event } = payload
          if (event === 'new') {
            // on destroy destroy client as well
            // console.log('NEW', payload)
            const { server } = payload
            if (addServer(selvaClient, <ServerDescriptor>server)) {
              selvaClient.emit('added-servers', payload)
            }
          } else if (event === 'remove') {
            // console.log('mofos remove', payload)
            const { server } = payload
            if (removeServer(selvaClient, <ServerDescriptor>server)) {
              selvaClient.emit('removed-servers', payload)
            }
          } else if (event === 'move-sub') {
            console.log('MOVE SUBSCRIPTION')
          } else if ('update-index') {
            // can be either a subs manager update of index or replica
            console.log('update index')
          }
        }
      })

      // add listeners
      selvaClient.emit('registry-started')

      console.log('ok made start of registry connection')
    }
  }
}
