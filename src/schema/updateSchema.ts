import { SelvaClient } from '../'
import { Schema, TypesDb, SearchIndexes } from '.'
import updateTypesId from './updateTypesId'
import updateTypeSchema from './updateTypeSchema'
import { getSchema } from './getSchema'
import { schemaCache, prefixesCache } from '../set/collectSchemas'

async function updateSchema(client: SelvaClient, props: Schema): Promise<void> {
  const { types, schema, searchIndexes } = await getSchema(client)

  // reset 5 second
  schemaCache.cache = {}
  prefixesCache.prefixes = false

  if (props.languages) {
    let changedstrings: boolean = false
    props.languages.forEach(lang => {
      // cannot remove languages for now!
      if (schema.languages.indexOf(lang) === -1) {
        schema.languages.push(lang)
        changedstrings = true
      }
    })
    if (changedstrings) {
      await client.redis.hset(
        '___selva_schema',
        'languages',
        JSON.stringify(props.languages)
      )
    }
  }

  if (props.types) {
    await updateTypesId(client, props.types, types)
    await updateTypeSchema(client, props.types, schema.types, searchIndexes)
  }

  // if change return true
}

export { updateSchema }
