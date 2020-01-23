import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaArrayLike } from '../../schema'
import fieldParsers from '.'

export default (
  schemas: Record<string, TypeSchema>,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaArrayLike,
  type: string
): void => {
  const arr = payload[field]

  if (!Array.isArray(arr)) {
    throw new Error(`Array is not an array ${JSON.stringify(arr)}`)
  }

  const itemsFields = fields.items
  const parser = fieldParsers[itemsFields.type]

  arr.forEach(payload => {
    parser(schemas, itemsFields, payload, result, fields, type)
  })

  result[field] = JSON.stringify(arr)
}
