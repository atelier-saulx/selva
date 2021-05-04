import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaObject } from '../../schema'
import fieldParsers from '.'

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: (string | Buffer)[],
  fields: FieldSchemaObject,
  type: string,
  $lang?: string
): Promise<number> => {
  if (typeof payload !== 'object' || Array.isArray(payload)) {
    throw new Error(`Incorrect payload for object ${JSON.stringify(payload)}`)
  }

  if (payload.$delete) {
    result.push('7', field, 'O')
    return 0
  }
  if (payload.$merge === false) {
    result.push('7', field, 'O')
  }

  let addedFields = 0
  for (let key in payload) {
    if (key[0] === '$') {
      if (key === '$merge') {
        // NOP
      } else if (key === '$ref') {
        result.push('0', field, payload[key])
        return 1
      } else if (key === '$delete') {
        // NOP - dead branch
      } else {
        throw new Error(`Wrong option on object ${key}`)
      }
    } else if (!fields.properties[key]) {
      throw new Error(`Cannot find field ${key} in ${type} for object`)
    } else {
      const item = fields.properties[key]
      const fn = fieldParsers[item.type]

      addedFields += await fn(
        client,
        schema,
        `${field}.${key}`,
        payload[key],
        result,
        fields.properties[key],
        type,
        $lang
      )
    }
  }

  if (addedFields) {
    const content = new Uint32Array([0])
    const buf = Buffer.from(content.buffer)
    result.push('C', field, buf)
  }

  return addedFields
}
