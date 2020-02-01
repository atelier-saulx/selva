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

function createAncestors(targetId: Id): Id[] {
  const s: Record<string, Ancestor> = {}
  createAncestorsInner(targetId, s)
  const result: Id[] = []
  // binary insert
  for (let id in s) {
    if (targetId !== id) {
      const depth = s[id][1]
      let l = 0,
        r = result.length - 1,
        m = 0
      while (l <= r) {
        m = Math.floor((l + r) / 2)
        const prevDepth = s[result[m]][1]
        if (prevDepth < depth) {
          r = m - 1
        } else {
          l = m + 1
          if (prevDepth === depth) {
            break
          }
        }
      }

      table.insert(result, l + 1, id)
    }
  }
  return result
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

function setFromAncestors(
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string,
  fieldFrom?: string | string[]
): boolean {
  const ancestorKey = id + '.ancestors'
  const parents = redis.smembers(id + '.parents')

  const queue: { id: Id; depthOffset: number }[] = []
  for (const parent of parents) {
    queue[queue.length] = { id: parent, depthOffset: 1 }
  }

  let currentDepthOffset: number = 1
  let currentMaxDepth: number = 0
  let currentMax: Id
  while (queue.length > 0) {
    // Array.prototype.unshift
    const ancestor = queue[0]
    table.remove(queue, 1)

    if (ancestor.depthOffset > currentDepthOffset) {
      if (currentMax) {
        // TODO: we're done
        break
      } else {
        currentDepthOffset = ancestor.depthOffset
      }
    }

    const zscore = redis.zscore(ancestorKey, ancestor.id)
    if (zscore && zscore > currentMaxDepth) {
      currentMaxDepth = zscore
      currentMax = ancestor.id
    }

    const parentsOfParent = redis.smembers(ancestor.id + '.parents')
    for (const parentOfParent of parentsOfParent) {
      queue[queue.length] = {
        id: parentOfParent,
        depthOffset: currentDepthOffset + 1
      }
    }
  }

  return false

  // shit
  // let deepestParent: Id
  // let deepestParentScore: number = 0
  // for (const parent of parents) {
  //   const parentScore = redis.zscore(ancestorKey, parent)
  //   if (parentScore > deepestParentScore) {
  //     deepestParent = parent
  //     deepestParentScore = parentScore
  //   }
  // }

  // if (deepestParent) {
  //   // TODO: set from this and check if true, and then return (if had what looking for)
  //   return true
  // }

  // // TODO: BFS recurse
  // for (const parent of parents) {
  //   if (
  //     setFromAncestors(
  //       result,
  //       schemas,
  //       parent,
  //       field,
  //       language,
  //       version,
  //       fieldFrom
  //     )
  //   ) {
  //     return true
  //   }
  // }

  // old
  // for (let i = 0, len = ancestors.length; i < len; i++) {
  //   if (fieldFrom && fieldFrom.length > 0) {
  //     if (
  //       getWithField(
  //         result,
  //         schemas,
  //         ancestors[i],
  //         field,
  //         fieldFrom,
  //         language,
  //         version
  //       )
  //     ) {
  //       break
  //     }
  //   } else {
  //     if (getByType(result, schemas, ancestors[i], field, language, version)) {
  //       break
  //     }
  //   }
  // }
}

function parseName(id: Id): string {
  return redis.hget(id, 'name')
}

function parseType(id: Id): string {
  return getTypeFromId(id)
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
  const ancestors = createAncestorsFromFields(id, item, parseType)
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
      setFromAncestors(result, schemas, id, field, language, version, fieldFrom)
    } else if (inherit.$type || inherit.$name) {
      let ancestors: Id[]
      if (inherit.$name) {
        inherit.$name = ensureArray(inherit.$name)
        ancestors = createAncestorsFromFields(id, inherit.$name, parseName)
      } else {
        inherit.$type = ensureArray(inherit.$type)
        ancestors = createAncestorsFromFields(id, inherit.$type, parseType)
      }
      setFromAncestors(result, schemas, id, field, language, version, fieldFrom)
    } else if (inherit.$item) {
      logger.info('inheriting with $item')
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
  }
}
