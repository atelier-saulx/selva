import { DeleteOptions } from './types'
import { SelvaClient } from '..'
import { SCRIPT } from '../constants'

async function deleteItem(
  client: SelvaClient,
  payload: DeleteOptions
): Promise<boolean> {
  const db = typeof payload === 'string' ? 'default' : payload.$db || 'default'
  const schema = client.schemas[db]

  return await client.redis.evalsha(
    { name: db || 'default', type: 'origin' },
    `${SCRIPT}:modify`,
    0,
    `${client.loglevel}:${client.uuid}`,
    schema.sha,
    JSON.stringify({
      kind: 'delete',
      payload
    })
  )
}

export { deleteItem, DeleteOptions }
