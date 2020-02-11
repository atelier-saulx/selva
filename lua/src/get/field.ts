import { GetResult } from '~selva/get/types'
import { Id } from '~selva/schema/index'
import getByType from './getByType'
import { Schema } from '../../../src/schema/index'
import * as logger from '../logger'
import { setNestedResult, getNestedField } from './nestedFields'
import { ensureArray } from 'lua/src/util'

function resolveVariable(
  id: Id,
  schema: Schema,
  variable: string,
  language?: string,
  version?: string
): string {
  const intermediateResult: object = {}
  getByType(intermediateResult, schema, id, variable, language, version)
  return getNestedField(intermediateResult, variable)
}

function resolveVariables(
  id: Id,
  schema: Schema,
  fieldDefinition: string,
  language?: string,
  version?: string
): string {
  let str = ''

  let inVariableDef = false
  let variable = ''
  for (let i = 0; i < fieldDefinition.length; i++) {
    if (!inVariableDef && fieldDefinition[i] === '$') {
      inVariableDef = true
    } else if (fieldDefinition[i] === '{' && inVariableDef) {
      // skip
    } else if (inVariableDef && fieldDefinition[i] === '}') {
      str += resolveVariable(id, schema, variable, language, version)
      variable = ''
      inVariableDef = false
    } else if (inVariableDef) {
      variable += fieldDefinition[i]
    } else {
      str += fieldDefinition[i]
    }
  }

  return str
}

export function resolveAll(
  id: Id,
  schema: Schema,
  fieldAry: string[],
  language?: string,
  version?: string
): string[] {
  const result: string[] = []
  for (let i = 0; i < fieldAry.length; i++) {
    result[i] = resolveVariables(id, schema, fieldAry[i], language, version)
  }

  return result
}

export default function getWithField(
  result: GetResult,
  schema: Schema,
  id: Id,
  field: string,
  $field: string | string[],
  language?: string,
  version?: string,
  includeMeta?: boolean
): boolean {
  const intermediateResult: object = {}
  let fromNested: any
  for (const fieldDefinition of $field) {
    if (
      getByType(
        intermediateResult,
        schema,
        id,
        fieldDefinition,
        language,
        version,
        includeMeta
      )
    ) {
      fromNested = getNestedField(intermediateResult, fieldDefinition)
      if (fromNested) {
        break
      }
    }
  }

  if (!fromNested) {
    return false
  }

  setNestedResult(result, field, fromNested)
  return true
}
