import { SelvaClient } from '.'
import { ServerSelector, ServerDescriptor, ServerSelectOptions } from './types'


const getServer = (
  selvaClient: SelvaClient,
  cb: (descriptor: ServerDescriptor) => void,
  selector: ServerSelector,
  selectionOptions?: ServerSelectOptions // channel for subscriptions
): void => {
  if (selector.host && selector.port) {
    cb({ ...selector, host: selector.host, port: selector.port })
  } else {
    let { type, name } = selector
    if (!selvaClient.registryConnection) {
      console.log(
        'registry connection not created, add once listener on selvaClient.connect (means registry is connected) '
      )
      selvaClient.addServerUpdateListeners.push(() => getServer(selvaClient, cb, selector, selectionOptions))
    } else {
      let server

      // this makes sense
      if (name && !type) {
        type = 'replica'
      }

      if (type === 'registry') {
        server = selvaClient.registryConnection.serverDescriptor
      } else if (type === 'origin') {
        if (!name) {
          name = 'default'
        }
        server = selvaClient.servers.origins[name]
      } else if (type === 'replica') {
        if (!name) {
          name = 'default'
        }
        const replicas = selvaClient.servers.replicas[name]
        server = replicas && replicas[0]
        if (
          !selector.strict &&
          !server &&
          (!selectionOptions || !selectionOptions.strict)
        ) {
          server = selvaClient.servers.origins[name]
        }
      }

      // for now only subscription
      if (selectionOptions) {
        // console.log('if selection options')
      }

      if (!server) {
        selvaClient.addServerUpdateListeners.push(() => {
          getServer(selvaClient, cb, selector, selectionOptions)
        })
      } else {
        cb(server)
      }
    }
  }
}

export default getServer
