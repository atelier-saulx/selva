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

  let hasKeys = false
  for (let key in payload) {
    if (key[0] === '$') {
      if (key === '$merge') {
        // TODO
        // if (!(payload[key] === true || payload[key] === false)) {
        //   throw new Error(`$merge needs to be a a boolean `)
        // }
        // r[key] = payload[key]
      } else if (key === '$ref') {
        r.push('0', field + '.' + key, payload[key])
        return
      } else if (key === '$delete') {
        // TODO
        // r.$delete = true
        // return
      } else {
        throw new Error(`Wrong option on object ${key}`)
      }
    } else {
      hasKeys = true
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
}
