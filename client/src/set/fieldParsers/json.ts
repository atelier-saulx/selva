import { SetOptions } from '../types'
import { Schema, FieldSchemaJson } from '../../schema'
import fieldParsers from '.'

export default (
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaJson,
  type: string,
  $lang?: string
): void => {
  if (!result.$args) result.$args = []

  if (payload.$delete) {
    result[field] = { $delete: true } // FIXME Remove
  } else if (payload.$ref) {
    // TODO: verify that it references a json field
    result[field] = `___selva_$ref:${payload.$ref}` // FIXME Remove
    result.$args.push('0', `${field}`, `___selva_$ref:${payload.$ref}`)
    return
  }

  if (fields.properties) {
    const obj = {}
    fieldParsers.object(
      schema,
      field,
      payload,
      obj,
      {
        type: 'object',
        properties: fields.properties
      },
      type,
      $lang
    )
    result[field] = JSON.stringify(obj) // FIXME Remoce
    result.$args.push('0', field, JSON.stringify(obj))
  } else {
    result[field] = JSON.stringify(payload) // FIXME remove
    result.$args.push('0', field, JSON.stringify(payload))
  }
  // needs nested json without casting to for exampe, json again
}
