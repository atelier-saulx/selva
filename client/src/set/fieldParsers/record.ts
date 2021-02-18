import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaRecord } from '../../schema'
import fieldParsers from '.'

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaRecord,
  type: string,
  $lang?: string
): Promise<void> => {
  if (typeof payload !== 'object' || Array.isArray(payload)) {
    throw new Error(`Incorrect payload for object ${JSON.stringify(payload)}`)
  }
  const r: string[] = []

  const fn = fieldParsers[fields.values.type]

  if (payload.$delete) {
    r.push('7', field, '')
    return
  }
  if (payload.$merge === false) {
    r.push('7', field, '')
  }

  for (let key in payload) {
    if (key[0] === '$') {
      if (key === '$merge') {
        // NOP
      } else if (key === '$ref') {
        r.push('0', field + '.' + key, payload[key])
        return
      } else if (key === '$delete') {
        // NOP - dead branch
      } else {
        throw new Error(`Wrong option on object ${key}`)
      }
    } else {
      await fn(
        client,
        schema,
        `${field}.${key}`,
        payload[key],
        r,
        fields.values,
        type,
        $lang
      )
      result.push(...r)
    }
  }

  const content = new Uint32Array([1])
  const buf = Buffer.from(content.buffer)
  result.push('C', field, buf)
}
