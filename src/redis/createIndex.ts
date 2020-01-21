import RedisClient from './'
import { SearchSchema } from '../schema/types'

const searchSchema = {}

const create = async (
  client: RedisClient,
  index: string,
  schema: SearchSchema
) => {
  const args = [index, 'SCHEMA']
  for (const field in schema) {
    args.push(field, ...schema[field])
  }
  try {
    return client.ftCreate(...args)
  } catch (e) {}
}

const alter = async (
  client: RedisClient,
  index: string,
  schema: SearchSchema
) => {
  try {
    return Promise.all(
      Object.keys(schema).map(field =>
        client.ftAlter(index, 'SCHEMA', 'ADD', field, ...schema[field])
      )
    )
  } catch (e) {}
}

const createIndex = async (client: RedisClient) => {
  const index = 'selva'
  try {
    const info = await client.ftInfo(index)
    const fields = info[info.indexOf('fields') + 1]
    const set = new Set()
    let changed = fields.find(([field, _, ...type]) => {
      const scheme = searchSchema[field]
      set.add(field)
      return scheme && scheme.find((key, i) => type[i] !== key)
    })
    if (!changed) {
      for (const field in searchSchema) {
        if (!set.has(field)) {
          changed = true
          break
        }
      }
    }
    if (changed) {
      return alter(client, index, searchSchema)
    }
  } catch (e) {
    if (/Unknown Index name/.test(e)) {
      return create(client, index, searchSchema)
    }
  }
}

export default createIndex
