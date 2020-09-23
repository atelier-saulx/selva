import { GetItem, GetResult, GetOptions } from '~selva/get/types'
import { Id } from '~selva/schema/index'
import getByType from './getByType'
import { Schema } from '../../../src/schema/index'
import * as logger from '../logger'
import { setNestedResult, getNestedField, setMeta } from './nestedFields'
import inherit from './inherit'
import getWithField, { resolveAll, isObjectField } from './field'
import getArray from './getArray'
import { getSchema } from '../schema/index'
import { ensureArray, isArray } from 'lua/src/util'
import makeNewGetOptions from './all'
import getQuery from './query/index'
import checkSingleReference from './reference'
import * as r from '../redis'

import global from '../globals'

// add error handling

function getField(
  props: GetItem,
  schema: Schema,
  result: GetResult,
  id: Id,
  field?: string,
  language?: string,
  version?: string,
  ignore?: '$' | '$inherit' | '$list' | '$find' | '$filter', // when from inherit
  metaKeys?: any
): boolean {
  let hasAlias = false

  if (props.$value) {
    setNestedResult(result, <string>field, props.$value)
    return true
  }

  if (props.$id && field) {
    if (props.$id.$field) {
      const idResult = {}
      getWithField(
        idResult,
        schema,
        id,
        'idResult',
        ensureArray(props.$id.$field),
        language,
        version
      )

      const nestedId = getNestedField(idResult, 'idResult')
      if (!nestedId) {
        setNestedResult(result, field, {})
        return true
      }

      props.$id = nestedId
    }

    const intermediateResult = {}
    getField(
      props,
      schema,
      intermediateResult,
      props.$id,
      undefined,
      language,
      version,
      ignore
    )

    setNestedResult(result, field, intermediateResult)

    return true
  }

  if (
    (props.$list || props.$find) &&
    ignore !== '$list' &&
    ignore !== '$' &&
    ignore !== '$find'
  ) {
    // field that needs to get the result

    let sourceField: string | string[] = field || ''
    let ids: string[] = [id]

    if (
      !(props.$list && typeof props.$list === 'object' && props.$list.$find) &&
      props.$field
    ) {
      if (isObjectField(props.$field)) {
        if (!props.$field.value.$id) {
          return false
        }

        const resolvedId = resolveId(ensureArray(props.$field.value.$id), [])
        sourceField = resolveAll(
          <string>resolvedId,
          schema,
          ensureArray(props.$field.path),
          language,
          version
        )
        ids = [<string>resolvedId]
      } else {
        sourceField = resolveAll(
          id,
          schema,
          ensureArray(props.$field),
          language,
          version
        )
      }
    }

    // clean up this property so we don't use it in gets with lists
    delete props.$field

    // allways need a field for getQuery
    const err = getQuery(
      getField,
      schema,
      result,
      props,
      field || '',
      ids,
      sourceField,
      language,
      version
    )
    if (err) {
      // can return an error now
      logger.error(err)
      error(err)
    }

    return true
  } else {
    if (props.$field && field) {
      hasAlias = true

      if (isObjectField(props.$field) && props.$field.value.$id) {
        const intermediateResult = {}
        const $field = resolveAll(
          id,
          schema,
          ensureArray(props.$field.path),
          language,
          version
        )

        getWithField(
          intermediateResult,
          schema,
          <string>resolveId(ensureArray(props.$field.value.$id), []),
          'intermediate',
          $field,
          language,
          version
        )

        setNestedResult(
          result,
          field,
          getNestedField(intermediateResult, 'intermediate')
        )

        return true
      } else {
        props.$field = resolveAll(
          id,
          schema,
          ensureArray(<string[]>props.$field),
          language,
          version
        )

        if (
          checkSingleReference(
            result,
            props,
            getField,
            id,
            props.$field,
            field,
            language,
            version,
            ignore,
            metaKeys
          )
        ) {
          return true
        }

        if (
          getWithField(
            result,
            schema,
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
    }

    let isComplete = true
    let hasKeys = false
    let propagatedInherit = false
    if (!hasAlias) {
      if (props.$all) {
        props = makeNewGetOptions(id, field || '', schema, props)
      }

      if (
        checkSingleReference(
          result,
          props,
          getField,
          id,
          field,
          field,
          language,
          version,
          ignore,
          metaKeys
        )
      ) {
        return true
      }

      for (const key in props) {
        if (key[0] !== '$') {
          hasKeys = true
          const f = field && field.length > 0 ? field + '.' + key : key
          if (props[key] === true) {
            if (
              !getByType(
                result,
                schema,
                id,
                f,
                language,
                version,
                false,
                metaKeys
              )
            ) {
              isComplete = false
            }
          } else if (props[key] === false) {
            // skip
          } else if (isArray(props[key])) {
            getArray(
              getField,
              props[key],
              schema,
              result,
              id,
              f,
              language,
              version,
              ignore
            )
          } else {
            if (props.$inherit === true) {
              propagatedInherit = true
              props[key].$inherit = true
            }

            if (
              getField(props[key], schema, result, id, f, language, version)
            ) {
              isComplete = false
            }

            if (field && field !== '') {
              const nested = getNestedField(result, field)
              if (
                !nested ||
                (type(nested) === 'table' && next(nested) === null)
              ) {
                setNestedResult(result, field, {})
              }
            }
          }
        }
      }
    }

    if (propagatedInherit) {
      delete props.$inherit
    }

    if (
      (!ignore || (ignore !== '$' && ignore !== '$inherit')) &&
      props.$inherit &&
      (!isComplete || !hasKeys)
    ) {
      // inheritance
      setMeta(
        'ancestors',
        metaKeys || {
          ___ids: id
        }
      )

      if (!hasAlias && !hasKeys) {
        const complete = getByType(
          result,
          schema,
          id,
          <string>field,
          language,
          version,
          false,
          metaKeys
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
            hasAlias ? <string[]>props.$field : undefined
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
          hasAlias ? <string[]>props.$field : undefined
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
        false,
        metaKeys
      )
      if (!complete) {
        setNestedResult(result, <string>field, props.$default)
      }
    }

    return isComplete
  }
}

function getRawAncestors(
  id: string,
  result: Record<string, true> = {}
): Record<string, true> {
  let parents = r.smembers(id + '.parents')

  for (let i = 0; i < parents.length; i++) {
    if (!result[parents[i]]) {
      result[parents[i]] = true
      getRawAncestors(parents[i], result)
    }
  }

  return result
}

function resolveId(ids: Id[], aliases: string[]): string | null {
  for (const id of ids) {
    if (r.exists(id)) {
      return id
    }
  }

  for (const alias of aliases) {
    const aliased = r.hget('___selva_aliases', alias)
    if (aliased && aliased.length > 0) {
      return aliased
    }

    // also check if the alias is a valid id
    if (r.exists(alias)) {
      return alias
    }
  }

  if (aliases.length === 0 && ids.length === 0) {
    return 'root'
  } else if (ids.length > 0) {
    return ids[0]
  } else {
    return null
  }
}

function get(opts: GetOptions): GetResult {
  const schema = getSchema()
  const result: GetResult = {}
  console.log('MIT')

  let {
    $version: version,
    $id: ids,
    $alias: aliases,
    $language: language,
    $includeMeta: includeMeta,
    $subscription: subscription,
    $rawAncestors: rawAncestors
  } = opts

  let id = resolveId(ensureArray(ids), ensureArray(aliases))

  if (!id) {
    result.$isNull = true
    return <any>result
  }

  // make the job of queries a bit easier
  opts.$id = id

  if (includeMeta) {
    global.$meta = {}
    result.$meta = global.$meta
  }

  console.log('SUBSCR', subscription)
  if (subscription) {
    global.$subscription = subscription
    console.log('GLOBAL', global)
  }

  getField(opts, schema, result, id, undefined, language, version)

  if (rawAncestors) {
    const obj = getRawAncestors(id)
    const arr: string[] = []
    for (const id in obj) {
      arr[arr.length] = id
    }
    result.rawAncestors = arr
  }

  if (id !== 'root' && !r.exists(id)) {
    result.$isNull = true
  }

  return <any>result
}

export default get
