import { getSchema } from '../schema/getSchema'
import { v4 as uuid } from 'uuid'
import { SelvaClient } from '..'
import { IdOptions } from 'lua/src/id'

function hash(str: string): number {
  var hash = 5381
  var i = str.length
  while (i) hash = (hash * 33) ^ str.charCodeAt(--i)
  return hash >>> 0
}

async function getIdPrefix(
  client: SelvaClient,
  { db, type }: IdOptions
): Promise<string> {
  const schemaResp = await getSchema(client, { name: db || 'default' })
  const schema = schemaResp.schema

  const typeSchema = schema.types[type]
  if (!typeSchema) {
    throw new Error(`Type ${type} does not exist in database ${db}`)
  }

  return typeSchema.prefix
}

export default async function genId(
  client: SelvaClient,
  idOpts: IdOptions
): Promise<string> {
  const prefix = await getIdPrefix(client, idOpts)
  return prefix + hash(uuid()).toString(16)
}

