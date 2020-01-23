import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaObject } from '../../schema'
import fieldParsers from '.'

export default (
  schemas: Record<string, TypeSchema>,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaObject,
  type: string
): void => {
  if (typeof payload !== 'object' || Array.isArray(payload)) {
    throw new Error(`Incorrect payload for object ${JSON.stringify(payload)}`)
  }
  const r = (result[field] = {})
  for (let key in payload) {
    if (key[0] === '$') {
      if (key === '$merge') {
        if (!(payload[key] === true || payload[key] === false)) {
          throw new Error(`$merge needs to be a a boolean `)
        }
        r[key] = payload[key]
      } else {
        throw new Error(`Wrong option on object ${key}`)
      }
    } else if (!fields.properties[key]) {
      throw new Error(`Cannot find field ${key} in ${type} for object`)
    } else {
      const item = fields.properties[key]
      const fn = fieldParsers[item.type]
      fn(schemas, key, payload[key], r, fields.properties[key], type)
    }
  }
}
