import { Id, Language } from '../schema'
import { GetItem, GetResult, GetOptions } from './types'
import { SelvaClient } from '..'
import getField from './getField'
import { setNestedResult } from './nestedFields'
import inherit from './inherit'

export async function getInner(
  client: SelvaClient,
  props: GetItem,
  result: GetResult,
  id: Id,
  field?: string,
  language?: Language,
  version?: string,
  ignore?: '$' | '$inherit' | '$list' | '$find' | '$filter' // when from inherit
): Promise<boolean> {
  let isComplete = true
  let hasKeys = false
  for (const key in props) {
    if (key[0] !== '$') {
      hasKeys = true
      const f = field ? field + '.' + key : key
      if (props[key] === true) {
        if (!(await getField(client, id, f, result, language, version))) {
          isComplete = false
        }
      } else {
        if (
          !(await getInner(
            client,
            props[key],
            result,
            id,
            f,
            language,
            version
          ))
        ) {
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
      const complete = await getField(
        client,
        id,
        field,
        result,
        language,
        version
      )
      if (!complete) {
        await inherit(client, id, field || '', props, result, language, version)
      }
    } else {
      await inherit(client, id, field || '', props, result, language, version)
    }
  }

  if (props.$default) {
    const complete = await getField(
      client,
      id,
      field,
      result,
      language,
      version
    )
    if (!complete) {
      setNestedResult(result, field, props.$default)
    }
  }

  return isComplete
}

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  const result: GetResult = {}
  const { $version: version, $id: id, $language: language } = props
  if (id) {
    await getInner(client, props, result, id, undefined, language, version)
  } else {
    // only find
  }
  return result
}

export { get, GetOptions, GetResult }
