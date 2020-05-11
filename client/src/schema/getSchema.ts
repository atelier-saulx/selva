import { SelvaClient } from '../'
import { rootDefaultFields } from './constants'
import { Schema, SearchIndexes, GetSchemaResult } from './types'
import { ServerSelector } from '../types'
import { wait } from '../util'

async function getSchema(
  client: SelvaClient,
  selector: ServerSelector,
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

  const dbName = await client.redis.getServerName(selector)

  const [fetchedTypes, fetchedIndexes] = await client.redis.hmget(
    selector,
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
      return getSchema(client, selector, ++retry)
    }
  }

  if (fetchedTypes) {
    schema = JSON.parse(fetchedTypes)
  }

  if (fetchedIndexes) {
    searchIndexes = JSON.parse(fetchedIndexes)
  }

  // client.schema = schema
  // client.searchIndexes = searchIndexes // FIXME: do we need this?

  return { schema, searchIndexes }
}

export { getSchema, GetSchemaResult }
