import { FieldSchemaObject } from '@saulx/selva'
import { createHash } from 'crypto'

export function isObjectLike(x: any): x is FieldSchemaObject {
  return !!(x && x.properties)
}

export function hash(str: string): string {
  const hashingFn = createHash('sha256')
  hashingFn.update(str)
  return hashingFn.digest('hex')
}
