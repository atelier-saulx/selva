import { SelvaClient } from '../'
import { GetOptions } from './types'

export default async function resolveId(
  client: SelvaClient,
  props: GetOptions
): Promise<string | undefined> {

  const id = props.$id || props.$alias
  if (!id) {
    return 'root'
  }

  const arr = Array.isArray(id) ? id : [id]
  const res = await client.redis.selva_resolve_nodeid({ name: props.$db || 'default' }, '___selva_hierarchy', props.$subscription || '', ...arr)

  if (res && props.$alias && props.$subscription) {
    await Promise.all(arr.map(async a =>
        // TODO the sub_id should be generated
        client.redis.selva_subscriptions_addalias({ name: props.$db || 'default' }, '___selva_hierarchy', props.$subscription, 1, a)
    ))
  }

  return res
}
