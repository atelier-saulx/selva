import { Id, TypeSchema } from '~selva/schema/index'
import { GetResult } from '~selva/get/types'
import * as redis from '../redis'
import getByType from '../get/getByType'
import { setNestedResult, getNestedField } from '../get/nestedFields'

export function resolveObjectRef(
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
) {
  const ref = redis.hget(id, '$ref')
  if (!ref || ref.length === 0) {
    return false
  }

  const intermediateResult = {}
  const found = getByType(
    intermediateResult,
    schemas,
    id,
    field,
    language,
    version
  )

  if (found) {
    setNestedResult(result, field, getNestedField(intermediateResult, ref))
    return true
  }

  return false
}

export function tryResolveSimpleRef(
  result: GetResult,
  id: Id,
  field: string,
  value: string
): boolean {
  if (!value || value.indexOf('___selva_$ref:') !== 0) {
    return false
  }

  const intermediateResult = {}
  return true
}
