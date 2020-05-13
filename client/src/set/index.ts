import { SetOptions } from './types'
import { SelvaClient } from '..'
import { v4 as uuid } from 'uuid'
import { SCRIPT } from '../constants'
import { setInBatches, MAX_BATCH_SIZE } from './batching'
import parseSetObject from './validate'

export async function _set(
  client: SelvaClient,
  payload: SetOptions,
  schemaSha: string,
  db?: string
): Promise<string> {
  return await client.redis.evalsha(
    { name: db || 'default' },
    `${SCRIPT}:modify`,
    0,
    `undefined:undefined`,
    schemaSha,
    JSON.stringify({
      kind: 'update',
      payload
    })
  )
}

async function set(client: SelvaClient, payload: SetOptions): Promise<string> {
  const schemaResp = await client.getSchema(payload.$db)
  const schema = schemaResp.schema
  const parsed = parseSetObject(payload, schema)

  if (parsed.$_itemCount > MAX_BATCH_SIZE) {
    const [id] = await setInBatches(schema, client, parsed, 0, {
      $_batchOpts: { batchId: uuid() },
      db: payload.$db
    })
    return id
  }

  return _set(client, parsed, schema.sha, payload.$db)
}

export { set, SetOptions }
