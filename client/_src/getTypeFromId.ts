import { SelvaClient } from '.'
import { Id } from './schema'

// needs to change
export const getTypeFromIdSync = (client: SelvaClient, id: Id): string => {
  return client.schema.prefixToTypeMapping[id.slice(0, 2)]
}

const getTypeFromId = async (client: SelvaClient, id: Id): Promise<string> => {
  if (!client.schema) {
    await client.getSchema()
  }

  return client.schema.prefixToTypeMapping[id.slice(0, 2)]
}

export default getTypeFromId
