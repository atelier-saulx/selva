import { SelvaClient } from '../'
import { rootDefaultFields } from './constants'
import { Schema, SearchIndexes, GetSchemaResult } from './types'
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
    rootType: { fields: rootDefaultFields },
    idSeedCounter: 0,
    prefixToTypeMapping: {}
  }

  let searchIndexes: SearchIndexes = {}

  const [fetchedTypes, fetchedIndexes] = await client.redis.hmget(
    selector,
    '___selva_schema',
    'types',
    'searchIndexes'
  )

  if (fetchedTypes) {
    schema = JSON.parse(fetchedTypes)
  }

  if (fetchedIndexes) {
    searchIndexes = JSON.parse(fetchedIndexes)
  }

  client.schemas[selector.name] = schema

  return { schema, searchIndexes }
}

export { getSchema, GetSchemaResult }
