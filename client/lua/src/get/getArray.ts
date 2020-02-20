import { GetItem, GetResult } from '~selva/get/types'
import { Schema } from '../../../src/schema/index'
import { Id } from '~selva/schema/index'
import { GetFieldFn } from './types'
import { setNestedResult, getNestedField } from 'lua/src/get/nestedFields'
import * as logger from '../logger'

export default function getArray(
  getField: GetFieldFn,
  props: GetItem[],
  schema: Schema,
  result: GetResult,
  id: Id,
  field: string,
  resultField: string,
  language?: string,
  version?: string,
  includeMeta?: boolean,
  ignore?: '$' | '$inherit' | '$list' | '$find' | '$filter' // when from inherit
): boolean {
  logger.info('GET ARRAY')
  const resultAry: GetResult[] = []
  for (let i = 0; i < props.length; i++) {
    const intermediateResult = {}
    getField(
      props[i],
      schema,
      intermediateResult,
      id,
      field,
      language,
      version,
      includeMeta,
      ignore
    )
    logger.info('intermediate result for field', field, intermediateResult)
    resultAry[i] = getNestedField(intermediateResult, field)
  }

  setNestedResult(result, resultField, resultAry)
  return true
}
