import { SelvaClient } from '.'
import { ServerSelector, ServerDescriptor, ServerSelectOptions } from './types'
import { waitUntilEvent } from './util'

const getServer = async (
  selvaClient: SelvaClient,
  selector: ServerSelector,
  selectionOptions?: ServerSelectOptions // channel
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
  } else if (type === 'replica') {
    if (!name) {
      name = 'default'
    }
    const replicas = selvaClient.servers.replicas[name]
    server = replicas && replicas[0]
    if (!server && (!selectionOptions || !selectionOptions.strict)) {
      server = selvaClient.servers.origins[name]
    }
  }

  // for now only subscription
  if (selectionOptions) {
    console.log('if selection options')
  }

  if (!server) {
    if (selvaClient.server && selvaClient.server.type === 'replica') {
      console.log('ok cant find for replica', selector)
    }

    await waitUntilEvent(selvaClient, 'added-servers')

    if (selvaClient.server && selvaClient.server.type === 'replica') {
      console.log('ok got dat event for replica server (after)')

      console.log('hello servers', selvaClient.servers)
    }

    return getServer(selvaClient, selector, selectionOptions)
  }

  return server
  // this is tmp
}

export default getServer
