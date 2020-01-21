import { SelvaClient } from '../'
import { Schema, TypesDb, SearchIndexes } from '.'
import updateTypes from './updateTypes'
import updateTypeSchema from './updateTypeSchema'

async function getFields(client: SelvaClient) {
  const typesRaw = await client.redis.get('types')
  const schemaRaw = await client.redis.get('schema')
  const searchIndexesRaw = await client.redis.get('searchIndexes')

  const types: TypesDb =
    typesRaw === null ? { idSize: 0 } : JSON.parse(typesRaw)

  const schema: Schema =
    schemaRaw === null
      ? {
          languages: [],
          types: {}
        }
      : JSON.parse(schemaRaw)

  const searchIndexes: SearchIndexes =
    searchIndexesRaw === null ? {} : JSON.parse(searchIndexesRaw)

  return { types, schema, searchIndexes }
}

async function updateSchema(
  client: SelvaClient,
  props: Schema
): Promise<boolean> {
  const { types, schema, searchIndexes } = await getFields(client)
  let changed = false
  // languages
  if (props.languages) {
    props.languages.forEach(lang => {
      if (schema.languages.indexOf(lang) === -1) {
        schema.languages.push(lang)
      }
    })
  }
  //types
  if (props.types) {
    await updateTypes(client, props.types, types)
    if (
      await updateTypeSchema(client, props.types, schema.types, searchIndexes)
    ) {
      changed = true
    }
  }

  // if change return true
  return true
}

export { updateSchema }
