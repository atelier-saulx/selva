import { SetOptions } from '../types'
import { Schema, FieldSchemaJson } from '../../schema'
import fieldParsers from '.'

export default (
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaJson,
  type: string
): void => {
  if (payload.$ref) {
    // TODO: verify that it references a json field
    result[field] = `___selva_$ref:${payload.$ref}`
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
      type
    )
    result[field] = JSON.stringify(obj)
  } else {
    result[field] = JSON.stringify(payload)
  }
  // needs nested json without casting to for exampe, json again
}
