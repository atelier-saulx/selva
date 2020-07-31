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

    for (const e of r) {
      const o = {}
      let j: number

      for (let i = 0; i < e.length; i += 3) {
        const jf = e[i + 1]
        const v = e[i + 2]

        const [_, jStr, f] = jf.match(/^(\d*)\.(.*)$/)
        if (!j) j = Number(jStr)

        o[f] = v
      }

      arrayResult[j] = o
    }
  }
  // nested json special!
  result.push('0', field, JSON.stringify(arrayResult))
}
