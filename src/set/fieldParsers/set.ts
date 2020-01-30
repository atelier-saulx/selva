import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaArrayLike } from '../../schema'
import fieldParsers from '.'

export default (
  _schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  _fields: FieldSchemaArrayLike,
  _type: string
): void => {
  // const arr = payload
  // if (!Array.isArray(arr)) {
  //   throw new Error(`Array is not an array ${JSON.stringify(arr)}`)
  // }
  // const itemsFields = fields.items
  // const parser = fieldParsers[itemsFields.type]
  // arr.forEach(payload => {
  //   // need to remove all options from nested fields!
  //   parser(schemas, itemsFields, payload, result, fields, type)
  // })
  result[field] = payload
}
