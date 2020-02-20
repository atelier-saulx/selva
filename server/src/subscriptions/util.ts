import { FieldSchemaObject } from '@selva/client'

export function isObjectLike(x: any): x is FieldSchemaObject {
  return !!(x && x.properties)
}

