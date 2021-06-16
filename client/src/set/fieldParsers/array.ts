import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaArrayLike } from '../../schema'
import fieldParsers from '.'

const ITEM_TYPES: Record<string, number> = {
  string: 0,
  int: 2,
  float: 1,
  number: 1,
  object: 4,
  record: 4,
}

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: (string | Buffer)[],
  fields: FieldSchemaArrayLike,
  type: string,
  $lang?: string
): Promise<number> => {
  const arr = payload
  if (!Array.isArray(arr)) {
    if (payload.$delete === true) {
      result.push('7', field, '')
      return 0
    } else if (payload.$push) {
      const itemType = ITEM_TYPES[fields.items.type]
      const content = new Uint32Array([itemType])
      const buf = Buffer.from(content.buffer)
      result.push('D', field, buf)

      const fieldWithIdx = `${field}[-1]`
      const itemsFields = fields.items
      const parser = fieldParsers[itemsFields.type]
      parser(
        client,
        schema,
        fieldWithIdx,
        payload.$push,
        result,
        itemsFields,
        type,
        $lang
      )
    } else if (payload.$unshift) {
      const itemType = ITEM_TYPES[fields.items.type]
      const content = new Uint32Array([itemType, 0])
      const buf = Buffer.from(content.buffer)
      result.push('E', field, buf)

      const fieldWithIdx = `${field}[0]`
      const itemsFields = fields.items
      const parser = fieldParsers[itemsFields.type]
      parser(
        client,
        schema,
        fieldWithIdx,
        payload,
        result,
        itemsFields,
        type,
        $lang
      )
    } else if (payload.$assign) {
      const idx = payload.$assign.$idx
      const value = payload.$assign.$value

      if (!Number.isInteger(idx) || !value) {
        throw new Error(
          `$assign missing $idx or $value property ${JSON.stringify(payload)}`
        )
      }

      const fieldWithIdx = `${field}[${idx}]`
      const itemsFields = fields.items
      const parser = fieldParsers[itemsFields.type]
      parser(
        client,
        schema,
        fieldWithIdx,
        value,
        result,
        itemsFields,
        type,
        $lang
      )
    } else if (payload.$insert) {
      const idx = payload.$insert.$idx
      const value = payload.$insert.$value

      const itemType = ITEM_TYPES[fields.items.type]
      const content = new Uint32Array([itemType, idx])
      const buf = Buffer.from(content.buffer)
      result.push('E', field, buf)

      if (!Number.isInteger(idx) || !value) {
        throw new Error(
          `$assign missing $idx or $value property ${JSON.stringify(payload)}`
        )
      }

      const fieldWithIdx = `${field}[${idx}]`
      const itemsFields = fields.items
      const parser = fieldParsers[itemsFields.type]
      parser(
        client,
        schema,
        fieldWithIdx,
        value,
        result,
        itemsFields,
        type,
        $lang
      )
    } else if (payload.$remove) {
      const content = new Uint32Array([payload.$remove.$idx])
      const buf = Buffer.from(content.buffer)
      result.push('F', field, buf)
    }
  } else {
    const itemsFields = fields.items
    const parser = fieldParsers[itemsFields.type]
    await Promise.all(
      arr.map((payload, index) => {
        const fieldWithIdx = `${field}[${index}]`
        parser(
          client,
          schema,
          fieldWithIdx,
          payload,
          result,
          itemsFields,
          type,
          $lang
        )
      })
    )
  }

  return 1
}
