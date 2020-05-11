import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaObject } from '../../schema'
import fieldParsers from '.'

export default (
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaObject,
  type: string,
  $lang?: string
): void => {
  if (typeof payload !== 'object' || Array.isArray(payload)) {
    throw new Error(`Incorrect payload for object ${JSON.stringify(payload)}`)
  }
  const r: SetOptions = (result[field] = {})

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
      } else if (key === '$_itemCount') {
        r.$ref = payload[key]
        return
      } else {
        throw new Error(`Wrong option on object ${key}`)
      }
    } else if (!fields.properties[key]) {
      throw new Error(`Cannot find field ${key} in ${type} for object`)
    } else {
      const item = fields.properties[key]
      const fn = fieldParsers[item.type]

      fn(schema, key, payload[key], r, fields.properties[key], type, $lang)
    }
  }
}
