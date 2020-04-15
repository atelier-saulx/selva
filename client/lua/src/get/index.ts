import { GetItem, GetResult, GetOptions } from '~selva/get/types'
import { Id } from '~selva/schema/index'
import getByType from './getByType'
import { Schema } from '../../../src/schema/index'
import * as logger from '../logger'
import { setNestedResult, getNestedField } from './nestedFields'
import inherit from './inherit'
import getWithField, { resolveAll, isObjectField } from './field'
import getArray from './getArray'
import { getSchema } from '../schema/index'
import { ensureArray, isArray } from 'lua/src/util'
import makeNewGetOptions from './all'
import getQuery from './query/index'
import * as r from '../redis'

// add error handling
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

  if (props.$value) {
    setNestedResult(result, <string>field, props.$value)
    return true
  }

  if (props.$id && field) {
    const intermediateResult = {}
    getField(
      props,
      schema,
      intermediateResult,
      props.$id,
      undefined,
      language,
      version,
      false,
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

    if (field) {
      let sourceField: string | string[] = field
      let ids: string[] = [id]

      if (
        !(
          props.$list &&
          typeof props.$list === 'object' &&
          props.$list.$find
        ) &&
        props.$field
      ) {
        if (isObjectField(props.$field)) {
          if (!props.$field.value.$id) {
            return false
          }

          sourceField = resolveAll(
            props.$field.value.$id,
            schema,
            ensureArray(props.$field.path),
            language,
            version
          )
          ids = [props.$field.value.$id]
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
        field,
        ids,
        sourceField,
        language,
        version,
        includeMeta
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
          props.$field.value.$id,
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
    }

    let isComplete = true
    let hasKeys = false
    let propagatedInherit = false
    if (!hasAlias) {
      if (props.$all) {
        props = makeNewGetOptions(id, field || '', schema, props)
      }

      for (const key in props) {
        if (key[0] !== '$') {
          hasKeys = true
          const f = field && field.length > 0 ? field + '.' + key : key
          if (props[key] === true) {
            if (
              !getByType(result, schema, id, f, language, version, includeMeta)
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
              includeMeta,
              ignore
            )
          } else {
            if (props.$inherit === true) {
              propagatedInherit = true
              props[key].$inherit = true
            }

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
          includeMeta,
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
        includeMeta
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

function get(opts: GetOptions): GetResult {
  const schema = getSchema()
  const result: GetResult = {}

  let {
    $version: version,
    $id: id,
    $alias: alias,
    $language: language,
    $includeMeta: includeMeta,
    $rawAncestors: rawAncestors
  } = opts

  if (alias) {
    const aliased = r.hget('___selva_aliases', alias)
    if (aliased && aliased.length > 0) {
      id = aliased
    } else {
      // try with $alias as $id
      delete opts.$alias
      opts.$id = alias
      return get(opts)
    }
  } else if (!id) {
    id = 'root'
  }

  if (includeMeta) {
    result.$meta = { $refs: {} }
  }

  getField(opts, schema, result, id, undefined, language, version, includeMeta)

  if (rawAncestors) {
    const obj = getRawAncestors(id)
    const arr: string[] = []
    for (const id in obj) {
      arr[arr.length] = id
    }
    result.rawAncestors = arr
  }

  return <any>result
}

export default get
