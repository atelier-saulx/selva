import { GetItem, GetResult, GetOptions } from '~selva/get/types'
import { Id } from '~selva/schema/index'
import getByType from './getByType'
import * as redis from '../redis'
import { TypeSchema } from '../../../src/schema/index'
import * as logger from '../logger'
import { setNestedResult } from './nestedFields'
import inherit from './inherit'

function getField(
  props: GetItem,
  schemas: Record<string, TypeSchema>,
  result: GetResult,
  id: Id,
  field?: string,
  language?: string,
  version?: string,
  ignore?: '$' | '$inherit' | '$list' | '$find' | '$filter' // when from inherit
): boolean {
  let isComplete = true
  let hasKeys = false
  for (const key in props) {
    if (key[0] !== '$') {
      hasKeys = true
      const f = field ? field + '.' + key : key
      if (props[key] === true) {
        if (!getByType(result, schemas, id, f, language, version)) {
          isComplete = false
        }
      } else {
        if (getField(props[key], schemas, result, id, f, language, version)) {
          isComplete = false
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
    if (!hasKeys) {
      logger.info(`getByType on ${field}`)
      const complete = getByType(
        result,
        schemas,
        id,
        <string>field,
        language,
        version
      )
      logger.info(`NEEDS INHERIT? COMPLETE: ${tostring(complete)}}`)
      if (!complete) {
        inherit(
          getField,
          props,
          schemas,
          result,
          id,
          <string>field,
          language,
          version
        )
      }
    } else {
      inherit(
        getField,
        props,
        schemas,
        result,
        id,
        <string>field,
        language,
        version
      )
    }
  }

  if (props.$default) {
    logger.info(`has a $default for field ${field || ''}`)
    const complete = getByType(
      result,
      schemas,
      id,
      <string>field,
      language,
      version
    )
    if (!complete) {
      logger.info(
        `NOT COMPLETE FOR ${field}, USING THE DEFAULT ${props.$default}`
      )
      setNestedResult(result, <string>field, props.$default)
    }
  }

  return isComplete
}

export default function get(opts: GetOptions): GetResult {
  const types: Record<string, TypeSchema> = {}
  const reply = redis.hgetall('___selva_types')
  for (let i = 0; i < reply.length; i += 2) {
    const type = reply[i]
    const typeSchema: TypeSchema = cjson.decode(reply[i + 1])
    types[type] = typeSchema
  }

  const result: GetResult = {}
  const { $version: version, $id: id, $language: language } = opts
  if (id) {
    getField(opts, types, result, id, undefined, language, version)
    logger.info(`FINAL RESULT ${cjson.encode(result)}`)
  } else {
    // TODO: queries
  }

  return <any>result
}
