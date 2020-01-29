import { SelvaClient } from '../'
import { Schema, SearchIndexes } from '.'

type GetSchemaResult = {
  schema: Schema
  searchIndexes: SearchIndexes
}

async function getSchema(client: SelvaClient): Promise<GetSchemaResult> {
  // TODO: this needs to use the new structure of schemas
  // only change I think needed is we drop langauges from here and we just
  // get types and searchIndexes -- unless we want to rename 'types' key to say 'typeSchema' or 'schema'
  let schema: Schema = {
    languages: [],
    types: {},
    idSeedCounter: 0
  }

  const fetched = await client.redis.hget('___selva_schema', 'types')
  const searchIndexes = await client.redis.hget(
    '___selva_schema',
    'searchIndexes'
  )

  if (fetched) {
    schema = JSON.parse(fetched)
  }

  this.schema = schema
  this.searchIndexes = searchIndexes // FIXME: do we need this?

  return { schema, searchIndexes }
}

export { getSchema, GetSchemaResult }
