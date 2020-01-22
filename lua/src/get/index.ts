import { GetItem, GetResult, GetOptions } from '~selva/get/types'
import { Id } from '~selva/schema/index'
import getByType from './getByType'
import * as redis from '../redis'
import { TypeSchema } from '../../../src/schema/index'
import * as logger from '../logger'

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
      const complete = getByType(
        result,
        schemas,
        id,
        <string>field,
        language,
        version
      )
      if (!complete) {
        // await inherit(client, id, field || '', props, result, language, version)
      }
    } else {
      // await inherit(client, id, field || '', props, result, language, version)
    }
  }

  if (props.$default) {
    const complete = getByType(
      result,
      schemas,
      id,
      <string>field,
      language,
      version
    )
    if (!complete) {
      // setNestedResult(result, field, props.$default)
    }
  }

  return isComplete
}

export default function get(opts: GetOptions): Promise<GetResult> {
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
  } else {
    // TODO: queries
  }

  return <any>result
}
