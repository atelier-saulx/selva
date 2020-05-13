import { DeleteOptions } from './types'
import { SelvaClient } from '..'
import { SCRIPT } from '../constants'

async function deleteItem(
  client: SelvaClient,
  payload: DeleteOptions
): Promise<boolean> {
  const db = typeof payload === 'string' ? 'default' : payload.$db || 'default'
  const schemaResp = await client.getSchema(db)

  const res = await client.redis.evalsha(
    { name: db || 'default' },
    `${SCRIPT}:modify`,
    0,
    `undefined:undefined`,
    schemaResp.schema.sha,
    JSON.stringify({
      kind: 'delete',
      payload
    })
  )

  return res[0]
}

export { deleteItem, DeleteOptions }
