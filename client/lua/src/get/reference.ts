import { setNestedResult, getNestedSchema } from './nestedFields'
import { GetFieldFn } from './types'
import * as r from '../redis'
import { GetOptions, GetResult } from '~selva/get/types'
import { getSchema } from '../schema/index'
import { ensureArray, stringStartsWith } from '../util'
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
  let isRef = false
  for (const f of ensureArray(field)) {
    const nestedSchema = getNestedSchema(id, f)
    if (nestedSchema && nestedSchema.type === 'reference') {
      isRef = true
    }
  }

  if (!isRef) {
    return false
  }

  let hasKeys = false
  if (!field || !resultField) {
    return false
  }

  for (const key in props) {
    if (!stringStartsWith(key, '$')) {
      hasKeys = true
      break
    }
  }

  // in this case we just return the id through getByType
  if (!hasKeys || props[resultField] === true) {
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
    if (props.$inherit) {
      const intermediateResult: GetResult = {}
      const q = {
        $id: id,
        fields: {}
      }

      for (const f of ensureArray(field)) {
        q.fields[f] = { $field: f, $inherit: props.$inherit }
      }

      getField(
        q,
        schema,
        intermediateResult,
        id,
        undefined,
        language,
        version,
        ignore,
        metaKeys
      )

      if (intermediateResult.fields) {
        for (const f of ensureArray(field)) {
          if (intermediateResult.fields[f]) {
            reference = intermediateResult.fields[f]
            $field = f
            break
          }
        }
      } else {
        return false
      }
    } else {
      return false
    }
  }

  if (!$field || !reference) {
    return false
  }

  const intermediateResult = {}

  getField(
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

  setNestedResult(result, <string>resultField, intermediateResult)

  return true
}
