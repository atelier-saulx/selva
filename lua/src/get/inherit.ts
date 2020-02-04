import * as redis from '../redis'
import { Id, TypeSchema } from '~selva/schema/index'
import { getTypeFromId } from '../typeIdMapping'
import { GetResult, GetItem } from '~selva/get/types'
import { setNestedResult } from './nestedFields'
import getByType from './getByType'
import { ensureArray } from '../util'
import * as logger from '../logger'
import getWithField from 'lua/src/get/field'

function setFromAncestors(
  getField: GetFieldFn,
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string,
  fieldFrom?: string | string[],
  condition?: (ancestor: Id) => boolean,
  ancestorsWithScores?: Id[],
  props?: GetItem
): boolean {
  const parents = redis.smembers(id + '.parents')

  if (!ancestorsWithScores) {
    ancestorsWithScores = redis.zrangeWithScores(id + '.ancestors')
  }

  const ancestorDepthMap: Record<Id, number> = {}

  for (let i = 0; i < ancestorsWithScores.length; i += 2) {
    ancestorDepthMap[ancestorsWithScores[i]] =
      tonumber(ancestorsWithScores[i + 1]) || 0
  }

  let validParents: Id[] = []
  for (let i = 0; i < parents.length; i++) {
    if (parents[i] !== 'root' && ancestorDepthMap[parents[i]]) {
      validParents[validParents.length] = parents[i]
    }
  }

  // we want to check parents from deepest to lowest depth
  table.sort(validParents, (a, b) => {
    if (!a) {
      return false
    } else if (!b) {
      return true
    }

    return ancestorDepthMap[a] > ancestorDepthMap[b]
  })

  while (validParents.length > 0) {
    const next: Id[] = []
    for (const parent of validParents) {
      if (!condition || (condition && condition(parent))) {
        if (fieldFrom && fieldFrom.length > 0) {
          if (
            getWithField(
              result,
              schemas,
              parent,
              field,
              fieldFrom,
              language,
              version
            )
          ) {
            return true
          }
        } else if (field === '') {
          if (
            getField(
              props || {},
              schemas,
              result,
              parent,
              '',
              language,
              version,
              '$inherit'
            )
          ) {
            return true
          }
        } else {
          if (getByType(result, schemas, parent, field, language, version)) {
            return true
          }
        }
      }

      const parentsOfParents = redis.smembers(parent + '.parents')
      for (const parentOfParents of parentsOfParents) {
        if (parentOfParents !== 'root' && ancestorDepthMap[parentOfParents]) {
          next[next.length] = parentOfParents
        }
      }
    }

    validParents = next
  }

  return false
}

function getName(id: Id): string {
  return redis.hget(id, 'name')
}

function inheritItem(
  getField: GetFieldFn,
  props: GetItem,
  schemas: Record<string, TypeSchema>,
  result: GetResult,
  id: Id,
  field: string,
  item: string[],
  language?: string,
  version?: string
) {
  // old stuff
  // const ancestors = createAncestorsFromFields(id, item, getTypeFromId)
  const ancestorsWithScores = redis.zrangeWithScores(id + '.ancestors')
  const len = ancestorsWithScores.length
  if (len === 0) {
    setNestedResult(result, field, {})
    return
  }

  let results: Record<string, Id[]> = {}
  for (const itemType of item) {
    results[itemType] = []
  }

  for (let i = ancestorsWithScores.length - 2; i >= 0; i -= 2) {
    const ancestorType = getTypeFromId(ancestorsWithScores[i])
    if (results[ancestorType]) {
      const matches = results[ancestorType]
      matches[matches.length] = ancestorsWithScores[i]
    }
  }

  const intermediateResult = {}
  for (const itemType of item) {
    const matches = results[itemType]
    if (matches.length === 1) {
      const intermediateResult = {}
      getField(
        props,
        schemas,
        intermediateResult,
        matches[0],
        '',
        language,
        version,
        '$inherit'
      )
      setNestedResult(result, field, intermediateResult)
      return
    } else if (matches.length > 1) {
      setFromAncestors(
        getField,
        intermediateResult,
        schemas,
        id,
        '',
        language,
        version,
        '',
        (ancestor: Id) => {
          for (const match of matches) {
            if (match === ancestor) {
              return true
            }
          }

          return false
        },
        ancestorsWithScores,
        props || {}
      )

      setNestedResult(result, field, intermediateResult)
      return
    }
  }

  // set empty result
  setNestedResult(result, field, {})
}

type GetFieldFn = (
  props: GetItem,
  schemas: Record<string, TypeSchema>,
  result: GetResult,
  id: Id,
  field?: string,
  language?: string,
  version?: string,
  ignore?: '$' | '$inherit' | '$list' | '$find' | '$filter' // when from inherit
) => boolean

export default function inherit(
  getField: GetFieldFn,
  props: GetItem,
  schemas: Record<string, TypeSchema>,
  result: GetResult,
  id: Id,
  field: string,
  language?: string,
  version?: string,
  fieldFrom?: string | string[]
) {
  logger.info(`INHERITING FIELD ${field}`)
  const inherit = props.$inherit
  if (inherit) {
    if (inherit === true) {
      return setFromAncestors(
        getField,
        result,
        schemas,
        id,
        field,
        language,
        version,
        fieldFrom
      )
    } else if (inherit.$type) {
      const types: string[] = ensureArray(inherit.$type)
      return setFromAncestors(
        getField,
        result,
        schemas,
        id,
        field,
        language,
        version,
        fieldFrom,
        (ancestor: Id) => {
          for (const type of types) {
            if (type === getTypeFromId(ancestor)) {
              return true
            }
          }

          return false
        }
      )
    } else if (inherit.$name) {
      const names: string[] = ensureArray(inherit.$name)
      return setFromAncestors(
        getField,
        result,
        schemas,
        id,
        field,
        language,
        version,
        fieldFrom,
        (ancestor: Id) => {
          for (const name of names) {
            if (name === getName(ancestor)) {
              return true
            }
          }

          return false
        }
      )
    } else if (inherit.$item) {
      logger.info('INHERITING with $item')
      inherit.$item = ensureArray(inherit.$item)
      inheritItem(
        getField,
        props,
        schemas,
        result,
        id,
        field,
        inherit.$item,
        language,
        version
      )
    }

    return false
  }
}
