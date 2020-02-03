import * as redis from '../redis'
import { Id, TypeSchema } from '~selva/schema/index'
import { getTypeFromId } from '../typeIdMapping'
import { GetResult, GetItem } from '~selva/get/types'
import { setNestedResult } from './nestedFields'
import getByType from './getByType'
import { ensureArray } from '../util'
import * as logger from '../logger'
import getWithField from 'lua/src/get/field'

type Ancestor = [Ancestor[], number]

// memoize this in lua (within one batch of gets)
// const ancestorMap = {} etc
function createAncestorsInner(id: Id, s: Record<Id, Ancestor>): Ancestor {
  // if memoized[id] -> get it
  if (s[id]) {
    return s[id]
  }
  const parents = redis.smembers(id + '.parents')
  const ancestor: Ancestor = [[], 0]
  if (parents.length) {
    ancestor[1] = 1
    let pd = 0
    for (let pId of parents) {
      const a = createAncestorsInner(pId, s)
      if (a[1] > pd) {
        pd = a[1]
        table.insert(a[0], 1, a)
      } else {
        a[0][a[0].length] = a
      }
    }
    ancestor[1] += pd
  }
  s[id] = ancestor
  return ancestor
}

function createAncestorsFromFields(
  targetId: Id,
  fields: string[],
  parse: (id: Id) => string
): Id[] {
  const s: Record<Id, Ancestor> = {}
  createAncestorsInner(targetId, s)
  const result = []
  for (let id in s) {
    if (targetId !== id) {
      const ancestor = s[id]
      // get type/name index , store it for faster lookup
      let ignore = false
      let value: string | null = null
      const iterCtx: any[] = ancestor
      if (ancestor.length === 2) {
        value = parse(id)
        if (value) {
          for (let i = 0, len = fields.length; i < len; i++) {
            if (fields[i] === value) {
              iterCtx[iterCtx.length] = i
              iterCtx[iterCtx.length] = value
              break
            } else if (i === len - 1) {
              ignore = true
            }
          }
        }
      }
      if (!ignore && value) {
        const depth = iterCtx[1]
        const index = iterCtx[2]
        const v = iterCtx[3]
        // binary insert
        let l = 0,
          r = result.length - 1,
          m = 0
        while (l <= r) {
          m = Math.floor((l + r) / 2)
          const prev: any = s[result[m]]
          const prevValue = prev[3]
          if (v === prevValue) {
            const prevDepth = prev[1]
            if (prevDepth < depth) {
              r = m - 1
            } else {
              l = m + 1
              if (prevDepth === depth) {
                break
              }
            }
          } else {
            const prevIndex = prev[2]
            if (prevIndex > index) {
              r = m - 1
            } else {
              l = m + 1
              if (prevIndex === index) {
                break
              }
            }
          }
        }
        table.insert(result, l + 1, id)
      }
    }
  }
  return result
}

function setFromAncestorsByType(
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  types: string[],
  field: string,
  language?: string,
  version?: string,
  fieldFrom?: string | string[]
): boolean {
  const ancestors = redis.zrange(id + '.ancestors')
  for (let i = ancestors.length - 1; i >= 0; i--) {
    const ancestorType = getTypeFromId(ancestors[i])

    let isType = false
    for (const type of types) {
      if (ancestorType === type) {
        isType = true
      }
    }

    if (isType) {
      if (fieldFrom && fieldFrom.length > 0) {
        if (
          getWithField(
            result,
            schemas,
            ancestors[i],
            field,
            fieldFrom,
            language,
            version
          )
        ) {
          return true
        }
      } else {
        if (
          getByType(result, schemas, ancestors[i], field, language, version)
        ) {
          return true
        }
      }
    }
  }

  return false
}

function setFromAncestors(
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string,
  fieldFrom?: string | string[],
  condition?: (ancestor: Id) => boolean
): boolean {
  logger.info(`setFromAncestors for id ${id}`)
  const parents = redis.smembers(id + '.parents')

  const ancestorsWithScores = redis.zrangeWithScores(id + '.ancestors')
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
  logger.info(`INHERIT ITEM FOR FIELD ${field}`)
  const ancestors = createAncestorsFromFields(id, item, getTypeFromId)
  const len = ancestors.length
  if (len === 0) {
    setNestedResult(result, field, {})
  } else {
    for (let i = 0; i < len; i++) {
      const intermediateResult = {}
      const isComplete = getField(
        props,
        schemas,
        intermediateResult,
        ancestors[i],
        '',
        language,
        version,
        '$inherit'
      )
      if (isComplete || i === len - 1) {
        setNestedResult(result, field, intermediateResult)
        break
      }
    }
  }
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
        result,
        schemas,
        id,
        field,
        language,
        version,
        fieldFrom
      )
    } else if (inherit.$type) {
      inherit.$type = ensureArray(inherit.$type)
      return setFromAncestorsByType(
        result,
        schemas,
        id,
        inherit.$type,
        field,
        language,
        version,
        fieldFrom
      )
    } else if (inherit.$name) {
      const names: string[] = ensureArray(inherit.$name)
      return setFromAncestors(
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
