import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, FieldSchemaRecord } from '../../schema'
import fieldParsers from '.'

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: (string | Buffer)[],
  fields: FieldSchemaRecord,
  type: string,
  $lang?: string
): Promise<number> => {
  if (typeof payload !== 'object' || Array.isArray(payload)) {
    throw new Error(`Incorrect payload for record ${JSON.stringify(payload)}`)
  }

  const fn = fieldParsers[fields.values.type]

  if (Object.keys(payload).filter((key) => /\./g.test(key)).length) {
    throw new Error('Cannot use "." in a record key')
  }

  if (payload.$delete) {
    result.push('7', field, '')
    return 0
  }

  if (
    client.validator &&
    !client.validator(schema, type, field.split('.'), payload, $lang)
  ) {
    throw new Error('Incorrect payload for "record" from custom validator')
  }

  if (payload.$merge === false) {
    result.push('7', field, '')
  }

  let addedFields = 0
  for (const key in payload) {
    if (key[0] === '$') {
      if (key === '$merge') {
        // NOP
      } else if (key === '$ref') {
        result.push('0', field + '.' + key, payload[key])
        return 1
      } else if (key === '$delete') {
        // NOP - dead branch
      } else {
        throw new Error(`Incorrect option on object ${key}`)
      }
    } else {
      addedFields += await fn(
        client,
        schema,
        `${field}.${key}`,
        payload[key],
        result,
        fields.values,
        type,
        $lang
      )
    }
  }

  if (addedFields) {
    const content = new Uint32Array([1])
    const buf = Buffer.from(content.buffer)
    result.push('C', field, buf)
  }

  return addedFields
}
