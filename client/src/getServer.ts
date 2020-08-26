import { SelvaClient } from '.'
import { ServerSelector, ServerDescriptor } from './types'

export default async (
  selvaClient: SelvaClient,
  selector: ServerSelector
): Promise<ServerDescriptor> => {
  // word iets simpler
  return { type: 'registry', name: 'snurf', host: '12.23.1', port: 1 }
}
