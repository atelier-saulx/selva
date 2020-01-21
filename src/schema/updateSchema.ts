import { SelvaClient } from '../'
import { Schema, TypesDb, SearchIndexes } from '.'
import updateTypes from './updateTypes'
import updateTypeSchema from './updateTypeSchema'

async function getFields(client: SelvaClient) {
  const [types, languages, searchIndexes] = await Promise.all(
    [
      {
        field: 'types',
        def: { idSize: 0 }
      },
      {
        field: 'languages',
        def: []
      },
      {
        field: 'searchIndexes',
        def: {}
      }
    ].map(async ({ field, def }) => {
      const result = await client.redis.hget('schema', field)
      return result === null ? def : JSON.parse(result)
    })
  )

  const schema: Schema = {
    languages,
    types: {}
  }

  const schemas = await client.redis.hgetall('types')

  if (schemas) {
    for (const type in schemas) {
      schema.types[type] = JSON.parse(schemas[type])
    }
    console.log('fun previous schemas', schema)
  }

  return { types, schema, searchIndexes }
}

async function updateSchema(client: SelvaClient, props: Schema): Promise<void> {
  const { types, schema, searchIndexes } = await getFields(client)
  let changedSchema = false
  // languages
  if (props.languages) {
    let changedLanguages: boolean = false
    props.languages.forEach(lang => {
      // cannot remove languages for now!
      if (schema.languages.indexOf(lang) === -1) {
        schema.languages.push(lang)
        changedLanguages = true
      }
    })
    if (changedLanguages) {
      await client.redis.hset(
        'schema',
        'languages',
        JSON.stringify(props.languages)
      )
    }
  }

  if (props.types) {
    await updateTypes(client, props.types, types)
    await updateTypeSchema(client, props.types, schema.types, searchIndexes)
  }

  // if change return true
}

export { updateSchema }
