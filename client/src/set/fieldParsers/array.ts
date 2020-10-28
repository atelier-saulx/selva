import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaArrayLike } from '../../schema'
import fieldParsers from '.'

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: string[],
  fields: FieldSchemaArrayLike,
  type: string,
  $lang?: string
): Promise<void> => {
  const arr = payload
  if (!Array.isArray(arr)) {
    if (payload.$delete === true) {
      // TODO
      // result[field] = { $delete: true }
      result.push('7', field, '')
      return
    } else if (payload.$field) {
      // TODO: verify that it references an array field
      // TODO
      // result[field] = `___selva_$ref:${payload[field]}`
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
    const r = []
    // need to remove all options from nested fields!
    await Promise.all(
      arr.map((payload, index) =>
        parser(client, schema, `${index}`, payload, r, itemsFields, type, $lang)
      )
    )

    arrayResult = arr
  }
  // nested json special!
  result.push('0', field, JSON.stringify(arrayResult))
}
