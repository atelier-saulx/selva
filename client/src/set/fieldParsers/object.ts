import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaObject } from '../../schema'
import fieldParsers from '.'

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaObject,
  type: string,
  $lang?: string
): Promise<void> => {
  if (typeof payload !== 'object' || Array.isArray(payload)) {
    throw new Error(`Incorrect payload for object ${JSON.stringify(payload)}`)
  }
  const r: SetOptions = (result[field] = {})

  if (!result.$args) result.$args = []

  let hasKeys = false
  for (let key in payload) {
    if (key[0] === '$') {
      if (key === '$merge') {
        if (!(payload[key] === true || payload[key] === false)) {
          throw new Error(`$merge needs to be a a boolean `)
        }
        r[key] = payload[key]
      } else if (key === '$field') {
        r.$field = payload[key]
        return
      } else if (key === '$ref') {
        r.$ref = payload[key]
        return
      } else if (key === '$delete') {
        r.$delete = true
        return
      } else if (key === '$_itemCount') {
        r.$_itemCount = payload[key]
        return
      } else {
        throw new Error(`Wrong option on object ${key}`)
      }
    } else if (!fields.properties[key]) {
      throw new Error(`Cannot find field ${key} in ${type} for object`)
    } else {
      hasKeys = true
      const item = fields.properties[key]
      const fn = fieldParsers[item.type]

      // TODO we could pass result directly
      fn(client, schema, `${field}.${key}`, payload[key], r, fields.properties[key], type, $lang)
      result.$args.push(...r.$args)

      // check if nested things have been removed because there are empty objects or the like
      if (Object.keys(result[field]).length === 0) {
        hasKeys = false
      }
    }
  }

  if (!hasKeys) {
    // omit completely empty objects, so they are not mistaken for arrays
    delete result[field]
  }
}
