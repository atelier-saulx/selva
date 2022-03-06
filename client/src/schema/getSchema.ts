import { SelvaClient } from '../'
import { rootDefaultFields } from './constants'
import { Schema, GetSchemaResult } from './types'
import { ServerSelector } from '../types'

// we want to remove
async function getSchema(
  client: SelvaClient,
  selector: ServerSelector
): Promise<GetSchemaResult> {
  let schema: Schema = {
    languages: [],
    types: {},
    sha: 'default',
    rootType: { fields: rootDefaultFields, prefix: 'ro' },
    idSeedCounter: 0,
    prefixToTypeMapping: {},
  }

  const [fetchedTypes] = await client.redis.hmget(
    selector,
    '___selva_schema',
    'types'
  )

  if (fetchedTypes) {
    schema = JSON.parse(fetchedTypes)
  }

  client.schemas[selector.name] = schema

  return { schema }
}

export { getSchema, GetSchemaResult }
