import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, FieldSchemaJson } from '../../schema'
import fieldParsers from '.'

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: string[],
  fields: FieldSchemaJson,
  type: string,
  $lang?: string
): Promise<void> => {
  if (payload.$delete) {
    result[field] = { $delete: true } // FIXME Remove
  } else if (payload.$ref) {
    // TODO: verify that it references a json field
    result.push('0', `${field}`, `___selva_$ref:${payload.$ref}`)
    return
  }

  if (fields.properties) {
    // TODO
    // const r: string[] = []
    // fieldParsers.object(
    //   client,
    //   schema,
    //   field,
    //   payload,
    //   r,
    //   {
    //     type: 'object',
    //     properties: fields.properties
    //   },
    //   type,
    //   $lang
    // )
    // result.push('0', field, JSON.stringify(obj))
  } else {
    result.push('0', field, JSON.stringify(payload))
  }
  // needs nested json without casting to for example, json again
}
