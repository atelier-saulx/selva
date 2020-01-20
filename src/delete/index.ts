import { Id } from '../schema'
import { DeleteOptions } from './types'
import { SelvaClient } from '..'

async function deleteItem(
  client: SelvaClient,
  id: Id,
  hierarchy: boolean = true
): Promise<boolean> {
  const modifyResult = await client.modify({
    kind: 'delete',
    payload: { $id: id, $hierarchy: hierarchy }
  })

  return modifyResult[0]
}

export { deleteItem, DeleteOptions }
