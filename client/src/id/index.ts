import { v4 as uuid } from 'uuid'
import { SelvaClient } from '..'
import { IdOptions } from 'lua/src/id'

function getIdPrefix(client: SelvaClient, { db, type }: IdOptions): string {
  const schema = client.schemas[db || 'default']

  const typeSchema = schema.types[type]
  if (!typeSchema) {
    throw new Error(`Type ${type} does not exist in database ${db}`)
  }

  return typeSchema.prefix
}

export default function genId(client: SelvaClient, idOpts: IdOptions): string {
  const prefix = getIdPrefix(client, idOpts)
  return prefix + uuid().substring(0, 8)
}
