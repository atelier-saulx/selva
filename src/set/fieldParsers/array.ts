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
  const arr = payload
  if (!Array.isArray(arr)) {
    if (payload.$field) {
      // TODO: verify that it references an array field
      result[field] = `___selva_$ref:${payload[field]}`
      return
    }
    throw new Error(`Array is not an array ${JSON.stringify(payload)}`)
  }
  const itemsFields = fields.items
  const parser = fieldParsers[itemsFields.type]
  const arrayResult = []
  arr.forEach((payload, index) => {
    // need to remove all options from nested fields!
    parser(schemas, index, payload, arrayResult, itemsFields, type)
  })
  // nested json special!
  result[field] = JSON.stringify(arrayResult)
}
