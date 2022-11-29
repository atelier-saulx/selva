import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, FieldSchemaArrayLike } from '../../schema'
import fieldParsers from '.'

const ITEM_TYPES: Record<string, number> = {
  string: 0,
  int: 2,
  float: 1,
  number: 1,
  object: 4,
  record: 4,
}

async function sendPush(
  typeArg: Buffer,
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: (string | Buffer)[],
  fields: FieldSchemaArrayLike,
  type: string,
  $lang?: string
) {
  result.push('D', field, typeArg)

  const fieldWithIdx = `${field}[-1]`
  const itemsFields = fields.items
  const parser = fieldParsers[itemsFields.type]
  return parser(
    client,
    schema,
    fieldWithIdx,
    payload,
    result,
    itemsFields,
    type,
    $lang
  )
}

async function sendInsert(
  typeArg: Buffer,
  idx: number,
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: (string | Buffer)[],
  fields: FieldSchemaArrayLike,
  type: string,
  $lang?: string
) {
  result.push('E', field, typeArg)

  if (!Number.isInteger(idx) || !payload) {
    throw new Error(
      `$assign missing $idx or $value property ${JSON.stringify(payload)}`
    )
  }

  const fieldWithIdx = `${field}[${idx}]`
  const itemsFields = fields.items
  const parser = fieldParsers[itemsFields.type]
  return parser(
    client,
    schema,
    fieldWithIdx,
    payload,
    result,
    itemsFields,
    type,
    $lang
  )
}

// add validation for array (missing)
/*
if (
    client.validator &&
    !client.validator(schema, type, field.split('.'), payload, $lang)
  ) {
    throw new Error('Incorrect payload for "json" from custom validator')
  }
*/

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
  // check where validation goes wrong for arrays...
  if (
    client.validator &&
    !client.validator(schema, type, field.split('.'), payload, $lang)
  ) {
    throw new Error('Incorrect payload for "array" from custom validator')
  }

  const arr = payload
  if (!Array.isArray(arr)) {
    if (payload.$delete === true) {
      result.push('7', field, '')
      return 0
    } else if (payload.$push !== undefined) {
      const itemType = ITEM_TYPES[fields.items.type]
      const content = new Uint32Array([itemType])
      const buf = Buffer.from(content.buffer)
      if (Array.isArray(payload.$push)) {
        for (const el of payload.$push) {
          await sendPush(
            buf,
            client,
            schema,
            field,
            el,
            result,
            fields,
            type,
            $lang
          )
        }
      } else {
        await sendPush(
          buf,
          client,
          schema,
          field,
          payload.$push,
          result,
          fields,
          type,
          $lang
        )
      }
    } else if (payload.$unshift !== undefined) {
      const itemType = ITEM_TYPES[fields.items.type]
      const content = new Uint32Array([itemType, 0])
      const buf = Buffer.from(content.buffer)
      if (Array.isArray(payload.$unshift)) {
        for (let i = payload.$unshift.length - 1; i >= 0; i--) {
          const v = payload.$unshift[i]
          await sendInsert(
            buf,
            0,
            client,
            schema,
            field,
            v,
            result,
            fields,
            type,
            $lang
          )
        }
      } else {
        await sendInsert(
          buf,
          0,
          client,
          schema,
          field,
          payload.$unshift,
          result,
          fields,
          type,
          $lang
        )
      }
    } else if (payload.$assign !== undefined) {
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
      await parser(
        client,
        schema,
        fieldWithIdx,
        value,
        result,
        itemsFields,
        type,
        $lang
      )
    } else if (payload.$insert !== undefined) {
      const idx = payload.$insert.$idx
      const itemType = ITEM_TYPES[fields.items.type]
      const content = new Uint32Array([itemType, idx])
      const buf = Buffer.from(content.buffer)
      if (Array.isArray(payload.$insert.$value)) {
        for (let i = payload.$insert.$value.length - 1; i >= 0; i--) {
          const v = payload.$insert.$value[i]
          await sendInsert(
            buf,
            idx,
            client,
            schema,
            field,
            v,
            result,
            fields,
            type,
            $lang
          )
        }
      } else {
        await sendInsert(
          buf,
          idx,
          client,
          schema,
          field,
          payload.$insert.$value,
          result,
          fields,
          type,
          $lang
        )
      }
    } else if (payload.$remove !== undefined) {
      const content = new Uint32Array([payload.$remove.$idx])
      const buf = Buffer.from(content.buffer)
      result.push('F', field, buf)
    } else {
      throw new Error(
        `Unknown operator for arrays in ${JSON.stringify(payload)}`
      )
    }
  } else {
    // always clear the array first
    result.push('7', field, '')

    const itemsFields = fields.items
    const parser = fieldParsers[itemsFields.type]
    await Promise.all(
      arr.map((payload, index) => {
        const fieldWithIdx = `${field}[${index}]`
        return parser(
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
