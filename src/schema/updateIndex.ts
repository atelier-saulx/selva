import RedisClient from '../redis'
import { SearchSchema } from '.'

export const create = async (
  client: RedisClient,
  index: string,
  schema: SearchSchema
) => {
  if (Object.keys(schema).length) {
    const args = [index, 'SCHEMA']
    for (const field in schema) {
      args.push(field, ...schema[field])
    }
    try {
      return client.ftCreate(...args)
    } catch (e) {}
  }
}

export const alter = async (
  client: RedisClient,
  index: string,
  schema: SearchSchema
) => {
  try {
    return Promise.all(
      Object.keys(schema).map(async field => {
        try {
          return await client.ftAlter(
            index,
            'SCHEMA',
            'ADD',
            field,
            ...schema[field]
          )
        } catch (e) {}
      })
    )
  } catch (e) {}
}

// index is a string here
const updateIndex = async (
  client: RedisClient,
  index: string = 'selva',
  schema: SearchSchema
) => {
  try {
    const info = await client.ftInfo(index)
    const fields = info[info.indexOf('fields') + 1]

    const set = new Set()
    let changed = fields.find(([field, _, ...type]) => {
      const scheme = schema[field]
      set.add(field)
      return scheme && scheme.find((key, i) => type[i] !== key)
    })
    if (!changed) {
      for (const field in schema) {
        if (!set.has(field)) {
          changed = true
          break
        }
      }
    }
    if (changed) {
      // if super different (e.g. fields differently indexed) then drop the index
      return alter(client, index, schema)
    }
  } catch (e) {
    if (/Unknown Index name/.test(e)) {
      return create(client, index, schema)
    }
  }
}

export default updateIndex
