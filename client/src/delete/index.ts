import { DeleteOptions } from './types'
import { SelvaClient } from '..'

async function deleteItem(
  client: SelvaClient,
  payload: DeleteOptions
): Promise<boolean> {
  const db = typeof payload === 'string' ? 'default' : payload.$db || 'default'
  const flags = typeof payload === 'string' ? '' : payload.$recursive ? 'F' : '';

  return client.redis.selva_hierarchy_del(
    { name: db, type: 'origin' },
    '___selva_hierarchy',
    flags,
    typeof payload === 'string' ? payload : payload.$id
  )
}

export { deleteItem, DeleteOptions }
