import { SelvaClient, ConnectOptions, ServerDescriptor } from '..'
import {
  createConnection,
  connections,
  uuid as connectionUuid
} from '../connection'
import { REGISTRY_UPDATE } from '../constants'
import getInitialRegistryServers from './getInitialRegistryServers'
import addServer from './addServer'
import removeServer from './removeServer'
import { serverId } from '../util'
import moveReplicas from './moveReplicas'

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

      // maybe for registry we want to handle it a bit different....
      registryConnection.attachSelvaClient(selvaClient)

      selvaClient.registryConnection = registryConnection

      registryConnection.subscribe(REGISTRY_UPDATE, selvaClient.selvaId)

      selvaClient.registryConnection.on('connect', () => {
        getInitialRegistryServers(selvaClient).then(() => {
          selvaClient.emit('added-servers', { event: '*' })
        })
      })

      // if a registry client is being re-used
      if (selvaClient.registryConnection.connected) {
        getInitialRegistryServers(selvaClient).then(() => {
          selvaClient.emit('added-servers', { event: '*' })
        })
      }

      const clear = () => {
        selvaClient.servers = {
          ids: new Set(),
          origins: {},
          subsManagers: [],
          replicas: {}
        }
        selvaClient.emit('removed-servers', { event: '*' })
      }

      selvaClient.registryConnection.on('destroy', clear)
      selvaClient.registryConnection.on('disconnect', clear)

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
            const { server } = payload
            if (removeServer(selvaClient, <ServerDescriptor>server)) {
              const id = serverId(server)
              const connection = connections.get(id)

              // if its from this we know to increase a counter for soft ramp up
              if (connection) {
                if (!connection.isDestroyed) {
                  console.log(
                    'found connection',
                    connectionUuid,
                    selvaClient.server
                  )
                  connection.hardDisconnect()
                } else {
                  console.log('allready destroyed!', id)
                }
              } else {
                // console.warn(
                //   'Removed a server - connection cannot be removed!',
                //   server
                // )
              }

              selvaClient.emit('removed-servers', payload)
            }
          } else if (event === 'move-sub') {
            console.log('MOVE SUBSCRIPTION')
          } else if ('update-index') {
            // now we are going to move them!
            // can be either a subs manager update of index or replica
            const { type, move } = payload
            if (type === 'replica') {
              moveReplicas(selvaClient, move)
            }
          }
        }
      })

      // add listeners
      selvaClient.emit('registry-started')

      console.log('ok made start of registry connection')
    }
  }
}
