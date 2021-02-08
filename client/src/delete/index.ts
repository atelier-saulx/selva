import { DeleteOptions } from './types'
import { SelvaClient } from '..'

async function deleteItem(
  client: SelvaClient,
  payload: DeleteOptions
): Promise<boolean> {
  const db = typeof payload === 'string' ? 'default' : payload.$db || 'default'

  return client.redis.selva_hierarchy_del(
    { name: db, type: 'origin' },
    '___selva_hierarchy',
    typeof payload === 'string' ? payload : payload.$id
  )
}

export { deleteItem, DeleteOptions }
