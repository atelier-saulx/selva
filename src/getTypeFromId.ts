import { SelvaClient } from '.'
import { Id } from './schema'

export const getPrefixes = async (client): Promise<Record<string, string>> => {
  let prefixes
  const r = await client.redis.hget('___selva_schema', 'prefixes')
  if (!r) {
    prefixes = {}
  } else {
    prefixes = JSON.parse(r)
  }
  return prefixes
}

export const getTypeFromIdSync = (
  client: SelvaClient,
  id: Id,
  prefixes: Record<string, string>
): string => {
  return prefixes[id.slice(0, 2)]
}

const getTypeFromId = async (
  client: SelvaClient,
  id: Id,
  prefixes?: Record<string, string>
): Promise<string> => {
  if (!prefixes) {
    prefixes = await getPrefixes(client)
  }
  return prefixes[id.slice(0, 2)]
}

export default getTypeFromId
