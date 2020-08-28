import { SelvaClient } from '.'
import { ServerSelector, ServerDescriptor } from './types'
import { waitUntilEvent } from './util'

const getServer = async (
  selvaClient: SelvaClient,
  selector: ServerSelector,
  selectionOptions?: { subscription?: string } // channel
): Promise<ServerDescriptor> => {
  if (selector.host && selector.port) {
    return { ...selector, host: selector.host, port: selector.port }
  }

  let { type, name } = selector

  if (!selvaClient.registryConnection) {
    console.log(
      'registry connection not created, add once listener on selvaClient.connect (means registry is connected) '
    )
    await waitUntilEvent(selvaClient, 'registry-started')
    return getServer(selvaClient, selector, selectionOptions)
  }

  let server
  if (type === 'registry') {
    server = selvaClient.registryConnection.serverDescriptor
  } else if (type === 'origin') {
    if (!name) {
      name = 'default'
    }
    server = selvaClient.servers.origins[name]
    console.log(selector, server)
  }

  // for now only subscription
  if (selectionOptions) {
    console.log('if selection options')
  }

  if (!server) {
    await waitUntilEvent(selvaClient, 'added-servers')
    return getServer(selvaClient, selector, selectionOptions)
  }

  return server
  // this is tmp
}

export default getServer
