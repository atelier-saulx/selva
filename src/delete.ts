import { Id } from './schema'
import { DeleteOptions } from './deleteTypes'
import { SelvaClient } from '.'

async function deleteItem(
  client: SelvaClient,
  id: Id,
  hierarchy: boolean = true
): Promise<boolean> {
  const modifyResult = await client.redis.modify({
    kind: 'delete',
    payload: id
  })

  return modifyResult[0]
}

export { deleteItem, DeleteOptions }
