import { DeleteOptions } from './types'
import { SelvaClient } from '..'

async function deleteItem(
  client: SelvaClient,
  payload: DeleteOptions
): Promise<boolean> {
  const modifyResult = await client.modify({
    kind: 'delete',
    payload
  })

  return <boolean>modifyResult
}

export { deleteItem, DeleteOptions }
