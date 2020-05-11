import { SelvaClient } from '../'
import { Schema, SearchIndexes, GetSchemaResult, rootDefaultFields } from '.'

const wait = (t: number = 0): Promise<void> =>
  new Promise(r => setTimeout(r, t))

async function getSchema(
  client: SelvaClient,
  retry: number = 0
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
    '___selva_schema',
    'types',
    'searchIndexes'
  )

  if (!fetchedTypes) {
    // means empty schema
    if (retry > 30) {
      console.log('max retries use default schema')
    } else {
      // console.log('no fetched types wait a bit')
      await wait(20)
      return getSchema(client, ++retry)
    }
  }

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
