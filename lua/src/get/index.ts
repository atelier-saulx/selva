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
import getQuery from './query/index'

// add error handling
function getField(
  props: GetItem,
  schema: Schema,
  result: GetResult,
  id: Id,
  field?: string,
  language?: string,
  version?: string,
  ignore?: '$' | '$inherit' | '$list' // when from inherit, or from find
): boolean {
  let hasAlias = false

  if (props.$list && ignore !== '$list' && ignore !== '$') {
    // field that needs to get the result

    if (field) {
      // allways need a field for getQuery
      const err = getQuery(
        getField,
        result,
        props,
        field,
        [id],
        field,
        language,
        version
      )
      if (err) {
        // can return an error now
        logger.error(err)
      }
    }
    return true
  } else {
    if (props.$field && field) {
      hasAlias = true

      props.$field = resolveAll(
        id,
        schema,
        ensureArray(props.$field),
        language,
        version
      )

      if (
        getWithField(result, schema, id, field, props.$field, language, version)
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
            if (!getByType(result, schema, id, f, language, version)) {
              isComplete = false
            }
          } else if (props[key] === false) {
            // skip
          } else {
            if (
              getField(props[key], schema, result, id, f, language, version)
            ) {
              isComplete = false
            }
          }
        }
      }
    }

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
        schema,
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
}

function get(opts: GetOptions): GetResult {
  const schema = getSchema()
  const result: GetResult = {}

  // logger.info(`GET ${cjson.encode(opts)}`)
  const { $version: version, $id: id = 'root', $language: language } = opts

  // default root
  getField(opts, schema, result, id, undefined, language, version)

  return <any>result
}

export default get
