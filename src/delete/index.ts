import { DeleteOptions } from './types'
import { SelvaClient } from '..'

async function deleteItem(
  client: SelvaClient,
  id: string,
  hierarchy: boolean = true
): Promise<boolean> {
  const modifyResult = await client.modify({
    kind: 'delete',
    payload: { $id: id, $hierarchy: hierarchy }
  })

  return <boolean>modifyResult
}

export { deleteItem, DeleteOptions }
