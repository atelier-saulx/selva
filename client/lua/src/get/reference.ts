import { setNestedResult, getNestedSchema } from './nestedFields'
import { GetFieldFn } from './types'
import * as r from '../redis'
import { GetOptions, GetResult } from '~selva/get/types'
import { getSchema } from '../schema/index'
import { ensureArray } from '../util'
import * as logger from '../logger'

export default function getSingularReference(
  result: GetResult,
  props: GetOptions,
  getField: GetFieldFn,
  id: string,
  field?: string | string[],
  resultField?: string,
  language?: string,
  version?: string,
  ignore?: '$' | '$inherit' | '$list' | '$find' | '$filter',
  metaKeys?: any
) {
  if (!field) {
    return false
  }

  const schema = getSchema()

  let reference: string | null = null
  let $field: string | null = null
  for (const f of ensureArray(field)) {
    const ref = r.hget(id, f)
    if (ref) {
      reference = ref
      $field = f
      break
    }
  }

  if (!$field || !reference) {
    return false
  }

  const nestedSchema = getNestedSchema(id, $field)
  if (nestedSchema && nestedSchema.type === 'reference') {
    const intermediateResult = {}

    const completed = getField(
      props,
      schema,
      intermediateResult,
      reference,
      undefined,
      language,
      version,
      ignore,
      metaKeys
    )

    if (!completed) {
      // TODO: $inherit?
    }

    setNestedResult(result, <string>resultField, intermediateResult)
    return true
  }

  return false
}
