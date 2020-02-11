import { GetItem, GetResult, GetOptions } from '~selva/get/types'
import { Id } from '~selva/schema/index'
import getByType from './getByType'
import { Schema } from '../../../src/schema/index'
import * as logger from '../logger'
import { setNestedResult } from './nestedFields'
import inherit from './inherit'
import getWithField, { resolveAll } from './field'
import { getSchema } from '../schema/index'
import { ensureArray } from 'lua/src/util'
import makeNewGetOptions from 'lua/src/get/all'

function getField(
  props: GetItem,
  schema: Schema,
  result: GetResult,
  id: Id,
  field?: string,
  language?: string,
  version?: string,
  includeMeta?: boolean,
  ignore?: '$' | '$inherit' | '$list' | '$find' | '$filter' // when from inherit
): boolean {
  let hasAlias = false
  if (props.$field && field) {
    hasAlias = true

    props.$field = resolveAll(
      id,
      schema,
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
        schema,
        id,
        field,
        props.$field,
        language,
        version,
        includeMeta
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
          if (
            !getByType(result, schema, id, f, language, version, includeMeta)
          ) {
            isComplete = false
          }
        } else if (props[key] === false) {
          // skip
        } else {
          if (
            getField(
              props[key],
              schema,
              result,
              id,
              f,
              language,
              version,
              includeMeta
            )
          ) {
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
        schema,
        id,
        <string>field,
        language,
        version,
        includeMeta
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
          includeMeta,
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
        includeMeta,
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
      schema,
      id,
      <string>field,
      language,
      version,
      includeMeta
    )
    if (!complete) {
      setNestedResult(result, <string>field, props.$default)
    }
  }

  return isComplete
}

export default function get(opts: GetOptions): GetResult {
  const schema = getSchema()
  const result: GetResult = {}

  // logger.info(`GET ${cjson.encode(opts)}`)
  const {
    $version: version,
    $id: id,
    $language: language,
    $includeMeta: includeMeta
  } = opts

  if (includeMeta) {
    result.$meta = { $refs: {} }
  }

  if (id) {
    getField(
      opts,
      schema,
      result,
      id,
      undefined,
      language,
      version,
      includeMeta
    )
  } else {
    // TODO: queries
  }

  return <any>result
}
