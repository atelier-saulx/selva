import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaArrayLike } from '../../schema'
import fieldParsers from '.'

export default (
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaArrayLike,
  type: string,
  $lang?: string
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
  let arrayResult = []

  if (itemsFields.type === 'json') {
    arrayResult = arr
  } else {
    const parser = fieldParsers[itemsFields.type]
    arr.forEach((payload, index) => {
      // need to remove all options from nested fields!
      parser(schema, index, payload, arrayResult, itemsFields, type, $lang)
    })
  }
  // nested json special!
  result[field] = JSON.stringify(arrayResult)
}
