import { Id, TypeSchema } from '~selva/schema/index'
import { GetResult } from '~selva/get/types'
import * as redis from '../redis'
import { setNestedResult, getNestedField } from '../get/nestedFields'

const REF_SIMPLE_FIELD_PREFIX = '___selva_$ref:'

type GetByTypeFn = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
) => boolean

export function resolveObjectRef(
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  getByType: GetByTypeFn,
  language?: string,
  version?: string
) {
  const ref = redis.hget(id, '$ref')
  if (!ref || ref.length === 0) {
    return false
  }

  return resolveRef(
    result,
    schemas,
    id,
    field,
    ref,
    getByType,
    language,
    version
  )
}

export function tryResolveSimpleRef(
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  value: string,
  getByType: GetByTypeFn,
  language?: string,
  version?: string
): boolean {
  if (!value || value.indexOf(REF_SIMPLE_FIELD_PREFIX) !== 0) {
    return false
  }

  const ref = value.substring(REF_SIMPLE_FIELD_PREFIX.length)
  return resolveRef(
    result,
    schemas,
    id,
    field,
    ref,
    getByType,
    language,
    version
  )
}

function resolveRef(
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  ref: string,
  getByType: GetByTypeFn,
  language?: string,
  version?: string
): boolean {
  const intermediateResult = {}
  const found = getByType(
    intermediateResult,
    schemas,
    id,
    ref,
    language,
    version
  )

  if (found) {
    const nested = getNestedField(intermediateResult, ref)
    if (nested) {
      setNestedResult(result, field, nested)
      return true
    }
  }

  return false
}
