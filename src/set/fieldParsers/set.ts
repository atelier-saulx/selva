import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaArrayLike } from '../../schema'

/*
const verifySimple = payload => {
  if (Array.isArray(payload)) {
    if (payload.find(v => typeof v !== 'string')) {
      throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
    }
    return payload
  } else if (typeof payload === 'string') {
    return [payload]
  } else {
    throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
  }
}
// check items
*/

export default (
  schemas: Record<string, TypeSchema>,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaArrayLike,
  type: string
): void => {
  // ensure array
  result[field] = payload[field]
}
