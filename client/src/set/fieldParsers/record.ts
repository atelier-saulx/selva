import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaRecord } from '../../schema'
import fieldParsers from '.'

export default (
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaRecord,
  type: string,
  $lang?: string
): void => {
  if (typeof payload !== 'object' || Array.isArray(payload)) {
    throw new Error(`Incorrect payload for object ${JSON.stringify(payload)}`)
  }
  const r: SetOptions = (result[field] = {})

  const fn = fieldParsers[fields.values.type]

  let hasKeys = false
  for (let key in payload) {
    hasKeys = true
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
      } else if (key === '$_itemCount') {
        r.$ref = payload[key]
        return
      } else {
        throw new Error(`Wrong option on object ${key}`)
      }
    } else {
      fn(schema, key, payload[key], r, fields.values, type, $lang)
    }
  }

  if (!hasKeys) {
    // omit completely empty objects, so they are not mistaken for arrays
    delete result[field]
  }
}
