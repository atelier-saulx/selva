import { SetOptions } from './types'
import { SelvaClient } from '..'
import { v4 as uuid } from 'uuid'
import { SCRIPT } from '../constants'
import { setInBatches, MAX_BATCH_SIZE } from './batching'
import parseSetObject from './validate'
import { getSchema } from 'lua/src/schema'

export async function _set(
  client: SelvaClient,
  payload: SetOptions,
  schemaSha: string,
  db?: string
): Promise<string> {
  return await client.redis.evalsha(
    { name: db || 'default', type: 'origin' },
    `${SCRIPT}:modify`,
    0,
    `${client.loglevel}:${client.uuid}`,
    schemaSha,
    JSON.stringify({
      kind: 'update',
      payload
    })
  )
}

async function set(client: SelvaClient, payload: SetOptions): Promise<string> {
  const schema = client.schemas[payload.$db || 'default']

  if (!payload.type && !payload.$id && payload.$alias) {
    let aliases = payload.$alias
    if (!Array.isArray(payload.$alias)) {
      aliases = [aliases]
    }

    for (const alias of aliases) {
      const id = await client.redis.hget(`___selva_aliases`, alias)
      if (id) {
        payload.$id = id
        break
      }
    }

    if (!payload.$id) {
      throw new Error(
        `.set() without the type property requires an existing record or $id to be set with the wanted type prefix. No existing id found for alias ${JSON.stringify(
          payload.$alias
        )}`
      )
    }
  }

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
