import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaObject } from '../../schema'
import fieldParsers from '.'

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaObject,
  type: string,
  $lang?: string
): Promise<void> => {
  if (typeof payload !== 'object' || Array.isArray(payload)) {
    throw new Error(`Incorrect payload for object ${JSON.stringify(payload)}`)
  }

  const r: string[] = []

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
        result.push('0', field, payload[key])
        return
      } else if (key === '$delete') {
        result.push('7', field, 'O')
        return
      } else {
        throw new Error(`Wrong option on object ${key}`)
      }
    } else if (!fields.properties[key]) {
      throw new Error(`Cannot find field ${key} in ${type} for object`)
    } else {
      hasKeys = true
      const item = fields.properties[key]
      const fn = fieldParsers[item.type]

      // TODO we could pass result directly
      await fn(
        client,
        schema,
        `${field}.${key}`,
        payload[key],
        r,
        fields.properties[key],
        type,
        $lang
      )
      result.push(...r)
    }
  }
}
