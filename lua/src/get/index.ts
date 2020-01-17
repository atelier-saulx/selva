import { GetOptions, GetResult } from '~selva/get'
import { GetItem } from '~selva/get/types'
import { Language, Id } from '~selva/schema'
import getByType from './getByType'

function getField(
  props: GetItem,
  result: GetResult,
  id: Id,
  field?: string,
  language?: Language,
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
        if (!getByType(result, id, f, language, version)) {
          isComplete = false
        }
      } else {
        if (getField(props[key], result, id, f, language, version)) {
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
      const complete = getByType(result, id, field, language, version)
      if (!complete) {
        // await inherit(client, id, field || '', props, result, language, version)
      }
    } else {
      // await inherit(client, id, field || '', props, result, language, version)
    }
  }

  if (props.$default) {
    const complete = getByType(result, id, field, language, version)
    if (!complete) {
      // setNestedResult(result, field, props.$default)
    }
  }

  return isComplete
}

export default function get(opts: GetOptions): Promise<GetResult> {
  const result: GetResult = {}
  const { $version: version, $id: id, $language: language } = opts
  if (id) {
    getField(opts, result, id, null, language, version)
  } else {
    // TODO: queries
  }

  return <any>result
}
