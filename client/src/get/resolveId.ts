import { SelvaClient } from '../'
import { GetOptions } from './types'

export default async function resolveId(
  client: SelvaClient,
  props: GetOptions
): Promise<string | undefined> {

  const id = props.$id || props.$alias
  if (!id) {
    return 'root';
  }

  const arr = Array.isArray(id) ? id : [id]
  return await client.redis.selva_resolve_nodeid({ name: props.$db || 'default' }, '___selva_hierarchy', ...arr)
}
