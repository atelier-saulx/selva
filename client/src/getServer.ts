import { SelvaClient } from '.'
import { ServerSelector, ServerDescriptor } from './types'

export default async (
  selvaClient: SelvaClient,
  selector: ServerSelector,
  selectionOptions?: { subscription?: string } // channel
): Promise<ServerDescriptor> => {
  if (selector.host || selector.port) {
    if (!selector.host) {
      selector.host = '0.0.0.0'
    }
    if (!selector.port) {
      selector.port = 6379
    }
    return { ...selector, host: selector.host, port: selector.port }
  } else if (selector.type === 'registry') {
    if (!selvaClient.registryConnection) {
      console.log(
        'registry connection not created, add once listener on selvaClient.connect (means registry is connected) '
      )
    } else {
      return selvaClient.registryConnection.serverDescriptor
    }
  }

  if (selectionOptions) {
    console.log('if selection options')
  }

  // this is tmp
  return selvaClient.registryConnection.serverDescriptor
}
