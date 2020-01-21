import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaOther } from '../../schema'

export default (
  schemas: Record<string, TypeSchema>,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaOther,
  type: string
): void => {
  console.log('😘  text', field, type)
  // ---
}
