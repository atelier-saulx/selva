import { SelvaClient } from '../'
import { Schema, SearchIndexes } from '.'

type GetSchemaResult = {
  schema: Schema
  searchIndexes: SearchIndexes
}

async function getSchema(client: SelvaClient): Promise<GetSchemaResult> {
  let schema: Schema = {
    languages: [],
    types: {},
    idSeedCounter: 0,
    prefixToTypeMapping: {}
  }

  let searchIndexes: SearchIndexes = {}

  const fetchedTypes = await client.redis.hget('___selva_schema', 'types')
  const fetchedIndexes = await client.redis.hget(
    '___selva_schema',
    'searchIndexes'
  )

  if (fetchedTypes) {
    schema = JSON.parse(fetchedTypes)
  }

  if (fetchedIndexes) {
    searchIndexes = JSON.parse(fetchedIndexes)
  }

  client.schema = schema
  client.searchIndexes = searchIndexes // FIXME: do we need this?

  return { schema, searchIndexes }
}

export { getSchema, GetSchemaResult }
