import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaArrayLike } from '../../schema'
import fieldParsers from '.'

export default (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaArrayLike,
  type: string,
  $lang?: string
): void => {
  if (!result.$args) result.$args = []

  const arr = payload
  if (!Array.isArray(arr)) {
    if (payload.$delete === true) {
      result[field] = { $delete: true }
    } else if (payload.$field) {
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
      parser(client, schema, `${index}`, payload, arrayResult, itemsFields, type, $lang)
    })
  }
  // nested json special!
  result.$args.push('0', field, JSON.stringify(arrayResult))
}
