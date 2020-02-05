import { GetItem, GetResult, GetOptions } from '~selva/get/types'
import { Id } from '~selva/schema/index'
import getByType from './getByType'
import * as redis from '../redis'
import { TypeSchema, Schema } from '../../../src/schema/index'
import * as logger from '../logger'
import { setNestedResult } from './nestedFields'
import inherit from './inherit'
import getWithField, { resolveAll } from './field'
import { getSchema } from '../schema/index'
import { ensureArray } from 'lua/src/util'
import { getTypeFromId } from 'lua/src/typeIdMapping'
import makeNewGetOptions from 'lua/src/get/all'

function getField(
  props: GetItem,
  schema: Schema,
  result: GetResult,
  id: Id,
  field?: string,
  language?: string,
  version?: string,
  ignore?: '$' | '$inherit' | '$list' | '$find' | '$filter' // when from inherit
): boolean {
  let hasAlias = false
  if (props.$field && field) {
    hasAlias = true

    props.$field = resolveAll(
      id,
      schema.types,
      ensureArray(props.$field),
      language,
      version
    )

    // logger.info(
    //   `$field is set, GETTING from ${props.$field} for field ${field}`
    // )
    if (
      getWithField(
        result,
        schema.types,
        id,
        field,
        props.$field,
        language,
        version
      )
    ) {
      return true
    }
  }

  let isComplete = true
  let hasKeys = false
  if (!hasAlias) {
    if (props.$all) {
      props = makeNewGetOptions(id, field || '', schema, props)
    }

    for (const key in props) {
      if (key[0] !== '$') {
        hasKeys = true
        const f = field && field.length > 0 ? field + '.' + key : key
        if (props[key] === true) {
          // logger.info(`key: ${key} field ${f}`)
          if (!getByType(result, schema.types, id, f, language, version)) {
            isComplete = false
          }
        } else {
          if (getField(props[key], schema, result, id, f, language, version)) {
            isComplete = false
          }
        }
      }
    }
  }

  // make no inherit a field - ignore field
  //
  if (
    (!ignore || (ignore !== '$' && ignore !== '$inherit')) &&
    props.$inherit &&
    (!isComplete || !hasKeys)
  ) {
    if (!hasAlias && !hasKeys) {
      const complete = getByType(
        result,
        schema.types,
        id,
        <string>field,
        language,
        version
      )
      if (!complete) {
        inherit(
          getField,
          props,
          schema,
          result,
          id,
          <string>field,
          language,
          version,
          hasAlias ? props.$field : undefined
        )
      }
    } else {
      inherit(
        getField,
        props,
        schema,
        result,
        id,
        <string>field,
        language,
        version,
        hasAlias ? props.$field : undefined
      )
    }
  }

  if (props.$default) {
    if (hasAlias) {
      setNestedResult(result, <string>field, props.$default)
      return true
    }

    const complete = getByType(
      result,
      schema.types,
      id,
      <string>field,
      language,
      version
    )
    if (!complete) {
      setNestedResult(result, <string>field, props.$default)
    }
  }

  return isComplete
}

export default function get(opts: GetOptions): GetResult {
  const schema = getSchema()
  const types: Record<string, TypeSchema> = schema.types
  const result: GetResult = {}

  // logger.info(`GET ${cjson.encode(opts)}`)
  const { $version: version, $id: id, $language: language } = opts
  if (id) {
    getField(opts, schema, result, id, undefined, language, version)
  } else {
    // TODO: queries
  }

  return <any>result
}
