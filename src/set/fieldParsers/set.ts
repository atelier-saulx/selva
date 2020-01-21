import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaArrayLike } from '../../schema'

export default (
  schemas: Record<string, TypeSchema>,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaArrayLike,
  type: string
): void => {
  console.log('😘  set', field, type)
  // ---
}
