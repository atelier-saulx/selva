import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaJson, FieldSchemaObject } from '../../schema'
import fieldParsers from '.'

export default (
  schemas: Record<string, TypeSchema>,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaJson,
  type: string
): void => {
  if (fields.properties) {
    fieldParsers.object(
      schemas,
      field,
      payload,
      result,
      {
        type: 'object',
        properties: fields.properties
      },
      type
    )
  }
  result[field] = JSON.stringify(payload)
}
