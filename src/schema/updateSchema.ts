import { SelvaClient } from '../'
import { Schema, TypesDb } from '.'
import updateTypes from './updateTypes'

async function getFields(client: SelvaClient) {
  const typesRaw = await client.redis.get('types')
  const schemaRaw = await client.redis.get('schema')
  const types: TypesDb =
    typesRaw === null ? { idSize: 0 } : JSON.parse(typesRaw)

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

  // languages
  if (props.languages) {
  }

  //types
  if (props.types) {
    await updateTypes(client, props.types, types)
  }

  // if change return true
  return true
}

export { updateSchema }
