import { SetOptions } from './types'
import { SelvaClient } from '..'
import { getPrefixes, getTypeFromIdSync } from '../getTypeFromId'
import { TypeSchema } from '../schema'

export let schemaCache: { cache: Record<string, TypeSchema> } = { cache: {} }
export let prefixesCache: { prefixes: false | Record<string, string> } = {
  prefixes: false
}

let schemaCacheTimer

const getSchema = async (
  client: SelvaClient,
  type: string
): Promise<TypeSchema> => {
  if (!schemaCache.cache[type]) {
    schemaCache.cache[type] = JSON.parse(
      await client.redis.hget('___selva_types', type)
    )
    if (!schemaCacheTimer) {
      schemaCacheTimer = true
      setTimeout(() => {
        schemaCacheTimer = false
        schemaCache.cache = {}
        prefixesCache.prefixes = false
      }, 5e3)
    }
  }
  return schemaCache.cache[type]
}

const getPrefixesCache = async (client: SelvaClient) => {
  if (!prefixesCache.prefixes) {
    prefixesCache.prefixes = await getPrefixes(client)
  }
  return prefixesCache.prefixes
}

const collectSchemas = async (
  client: SelvaClient,
  payload: SetOptions,
  schemas: Record<string, TypeSchema>,
  prefixes?: Record<string, string>
): Promise<Record<string, TypeSchema>> => {
  if (!payload.type) {
    prefixes = prefixesCache.prefixes || (await getPrefixesCache(client))
    payload.type = getTypeFromIdSync(client, payload.$id, prefixes)
  }

  // not using cached for async deep children while a set happens on the schemas...
  // maybe not nessecary

  const schema =
    schemas[payload.type] ||
    (schemas[payload.type] = schemaCache.cache[payload.type]) ||
    (schemas[payload.type] = await getSchema(client, payload.type))

  if (!schemaCache.cache._languages) {
    schemaCache.cache._languages = JSON.parse(
      await client.redis.hget('___selva_schema', 'languages')
    )
  }

  schemas._languages = schemaCache.cache._languages

  if (!schema) {
    throw new Error(`No schema defined for ${payload.type}`)
  }
  for (const key in payload) {
    if (key[0] === '$') {
      continue
    }
    if (!schema.fields[key]) {
      throw new Error(`Field ${key} not in schema ${payload.type}`)
    } else if (schema.fields[key].type === 'references') {
      if (Array.isArray(payload[key])) {
        if (typeof payload[key][0] === 'object') {
          try {
            await Promise.all(
              payload[key].map(child =>
                collectSchemas(client, child, schemas, prefixes)
              )
            )
          } catch (err) {
            throw err
          }
        }
      } else if (payload[key].$add && typeof payload[key].$add === 'object') {
        try {
          await collectSchemas(client, payload[key].$add, schemas, prefixes)
        } catch (err) {
          throw err
        }
      }
    }
  }
  return schemas
}

export default collectSchemas
