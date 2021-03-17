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
      result.push('7', field, '')
    } else if (payload.$push) {
      result.push('D', field, '')
    } else if (payload.$unshift) {
      result.push('E', field, '')
    } else if (payload.$assign) {
      // TODO: $merge: true/false (true default)
      // TODO: append index markers for commands
    } else if (payload.$remove) {
      result.push('F', field, `${payload.$remove.$idx}`)
    }

    return
  } else {
    const itemsFields = fields.items
    const parser = fieldParsers[itemsFields.type]
    const r = []

    await Promise.all(
      arr.map((payload, index) =>
        parser(client, schema, `${index}`, payload, r, itemsFields, type, $lang)
      )
    )

    // TODO: append index markers for commands
  }
}
