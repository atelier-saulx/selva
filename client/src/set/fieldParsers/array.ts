import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaArrayLike } from '../../schema'
import fieldParsers from '.'

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaArrayLike,
  type: string,
  $lang?: string
): Promise<void> => {
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
    const r = [];
    // need to remove all options from nested fields!
    await Promise.all(arr.map((payload, index) => parser(client, schema, `${index}`, payload, r, itemsFields, type, $lang)))

    /*
     * Parse the $args format
     */
    for (const e of r) {
      const o = {}
      let j: number

      for (let i = 0; i < e.$args.length; i += 3) {
        const jf = e.$args[i + 1]
        const v = e.$args[i + 2]

        const [_, jStr, f] = jf.match(/^(\d*)\.(.*)$/)
        if (!j) j = Number(jStr)

        o[f] = v
      }

      arrayResult[j] = o
    }
  }
  // nested json special!
  result.$args.push('0', field, JSON.stringify(arrayResult))
}
