import { FieldSchemaObject } from '../../../client/src/schema'

export function isObjectLike(x: any): x is FieldSchemaObject {
  return !!(x && x.properties)
}

