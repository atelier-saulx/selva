import { SelvaClient } from '../'
import { Schema } from './types'

async function updateSchema(
  client: SelvaClient,
  props: Schema
): Promise<boolean> {
  const ids = await client.redis.get('ids')
  const schema = await client.redis.get('schema')
  console.log(ids, schema)
  return true
}

export { updateSchema }
