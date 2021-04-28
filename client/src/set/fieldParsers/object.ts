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
): Promise<void> => {
  if (typeof payload !== 'object' || Array.isArray(payload)) {
    throw new Error(`Incorrect payload for object ${JSON.stringify(payload)}`)
  }

  // const r: string[] = []

  if (payload.$delete) {
    result.push('7', field, 'O')
    return
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
        return
      } else if (key === '$delete') {
        // NOP - dead branch
      } else {
        throw new Error(`Wrong option on object ${key}`)
      }
    } else if (!fields.properties[key]) {
      throw new Error(`Cannot find field ${key} in ${type} for object`)
    } else {
      addedFields++
      const item = fields.properties[key]
      const fn = fieldParsers[item.type]

      await fn(
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

  // result.push(...r)

  if (addedFields /* && r.length */) {
    const content = new Uint32Array([0])
    const buf = Buffer.from(content.buffer)
    result.push('C', field, buf)
  }
}
