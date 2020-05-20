import { v4 as uuid } from 'uuid'
import { SelvaClient } from '..'
import { IdOptions } from 'lua/src/id'

function hash(str: string): number {
  var hash = 5381
  var i = str.length
  while (i) hash = (hash * 33) ^ str.charCodeAt(--i)
  return hash >>> 0
}

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
  return prefix + hash(uuid()).toString(16)
}

