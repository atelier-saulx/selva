import { SelvaClient } from '../'
import { Schema } from '.'

async function updateSchema(
  client: SelvaClient,
  props: Schema
): Promise<boolean> {
  const idsRaw = await client.redis.get('ids')
  const schemaRaw = await client.redis.get('schema')
  let ids = idsRaw === null ? {} : JSON.parse(idsRaw)
  let schema = schemaRaw === null ? {} : JSON.parse(schemaRaw)

  console.log(ids, schema)

  return true
}

export { updateSchema }
