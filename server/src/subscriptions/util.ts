import { FieldSchemaObject } from '@saulx/selva'

export function isObjectLike(x: any): x is FieldSchemaObject {
  return !!(x && x.properties)
}

