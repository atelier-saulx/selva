import { SelvaClient } from '../'
import { Schema } from '.'

async function getFields(client: SelvaClient) {
  const typesRaw = await client.redis.get('types')
  const schemaRaw = await client.redis.get('schema')
  const types = typesRaw === null ? {} : JSON.parse(typesRaw)
  const schema: Schema =
    schemaRaw === null
      ? {
          languages: ['en'],
          types: {}
        }
      : JSON.parse(schemaRaw)

  return { types, schema }
}

async function updateSchema(
  client: SelvaClient,
  props: Schema
): Promise<boolean> {
  const { types, schema } = await getFields(client)

  console.log('types:', types, 'schema:', schema)

  console.log(props)

  // languages
  if (props.languages) {
  }

  // types
  if (props.types) {
    for (let type in props.types) {
      if (!types[type]) {
        console.log('new type go', type)
      }
    }
  }

  // if change return true
  return true
}

export { updateSchema }
